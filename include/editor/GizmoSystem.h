#pragma once

#include <core/SceneScopeSystem.h>
#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/CameraAsset3dInstance.h>
#include <core/data/assets/MaterialData.h>
#include <editor/ObjectSelectorSystem.h>

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

namespace lite {

// Uma peça do gizmo: os DADOS do modelo (não a instância) + seus materiais.
// Move-only: Asset3dData não é copiável nem movível (tem
// vector<unique_ptr<Asset3dData>> children e destrutor declarado), então a peça
// carrega o ponteiro e a posse.
struct GizmoPart {
    std::unique_ptr<Asset3dData> data;
    std::vector<MaterialData> materials;
};

// Os 9 modelos que compõem o gizmo — um por eixo de cada modo. Importados pela
// main (Assimp) e entregues ao sistema no construtor; o sistema é quem faz os
// create(), já na render thread. Move-only (ver GizmoPart).
struct GizmoParts {
    GizmoPart moveX, moveY, moveZ;
    GizmoPart rotateX, rotateY, rotateZ;
    GizmoPart scaleX, scaleY, scaleZ;
};

enum GizmoAction : size_t {
    MOVE_X = 0, MOVE_Y, MOVE_Z,
    ROTATE_X, ROTATE_Y, ROTATE_Z,
    SCALE_X, SCALE_Y, SCALE_Z,
    SLOT_COUNT
};

// Enum em C++ não tem método: o equivalente é função livre no mesmo namespace,
// que a ADL encontra sem qualificar (toString(action)). SLOT_COUNT é sentinela
// de contagem, não uma ação — cai no "UNKNOWN" junto com valores inválidos.
constexpr std::string_view toString(GizmoAction action) noexcept {
    switch (action) {
        case MOVE_X:   return "MOVE_X";
        case MOVE_Y:   return "MOVE_Y";
        case MOVE_Z:   return "MOVE_Z";
        case ROTATE_X: return "ROTATE_X";
        case ROTATE_Y: return "ROTATE_Y";
        case ROTATE_Z: return "ROTATE_Z";
        case SCALE_X:  return "SCALE_X";
        case SCALE_Y:  return "SCALE_Y";
        case SCALE_Z:  return "SCALE_Z";
        default:       return "UNKNOWN";
    }
}

// Permite `std::cout << action` — também achado por ADL.
inline std::ostream& operator<<(std::ostream& os, GizmoAction action) {
    return os << toString(action);
}

// Eixo de MUNDO da ação (unitário), independente do modo: MOVE_X, ROTATE_X e
// SCALE_X compartilham o eixo X. SLOT_COUNT e valores inválidos → vetor nulo.
// (Quando existir gizmo em espaço local, este é o ponto que passa a devolver a
// coluna correspondente da world matrix do objeto.)
inline glm::vec3 axisOf(GizmoAction action) noexcept {
    switch (action) {
        case MOVE_X: case ROTATE_X: case SCALE_X: return {1.0f, 0.0f, 0.0f};
        case MOVE_Y: case ROTATE_Y: case SCALE_Y: return {0.0f, 1.0f, 0.0f};
        case MOVE_Z: case ROTATE_Z: case SCALE_Z: return {0.0f, 0.0f, 1.0f};
        default:                                  return {0.0f, 0.0f, 0.0f};
    }
}

// Sistema do gizmo de transformação — renderer agnostic.
//
// Ao contrário dos outros systems, este é DONO DE UMA CENA: o gizmo é desenhado
// num overlay próprio (cena + view separadas da cena 3D), composto por cima dela
// dentro do mesmo frame. Isso mantém o gizmo sempre visível (não é ocluído pela
// geometria) e fora do picking da cena principal. A base cuida do ciclo; o
// renderer concreto (ex.: FilamentGizmoSystem) fornece as 6 operações virtuais.
//
// SceneType/TransformType: mesmo idioma do ObjectSelectorSystem<SceneType,
// TransformType> — são necessários porque a base possui o selector do gizmo
// (picking dos eixos), que é template sobre esses dois tipos. Com eles a base
// também consegue percorrer os nós (Asset3dInstance<TransformType>), então o
// intersectGizmo inteiro é agnóstico.
//
// Ciclo (ambos os hooks executam NA RENDER THREAD):
//   onFrameBegin     → FORA do frame GPU: 1ª vez initializeOverlay() +
//                      createRoot() + createPart() × 9; todo frame
//                      updateOverlay(dt) (instancia o que está na fila);
//                      1ª vez attachPartToRoot()
//   postRenderScene  → DENTRO do frame: renderOverlay()
//
// A separação instanciar-fora / desenhar-dentro é OBRIGATÓRIA, não estética:
// FEngine::prepare() roda uma vez por frame dentro do beginFrame() e é ele quem
// faz o commit de todas as MaterialInstance. Uma instância criada depois do
// beginFrame não passa por esse commit, e o descriptor set dela chega ao render
// pass sem handle → assert 'mDescriptorSetHandle' no Filament. Por isso nada
// que crie recurso pode rodar no postRenderScene.
template <typename SceneType, TransformConcept TransformType>
class GizmoSystem : public SceneScopeSystem {
public:
    using NodeType = Asset3dInstance<TransformType>;
    using SelectorType = ObjectSelectorSystem<SceneType, TransformType>;

    explicit GizmoSystem(GizmoParts parts) {
        m_parts[MOVE_X]   = std::move(parts.moveX);
        m_parts[MOVE_Y]   = std::move(parts.moveY);
        m_parts[MOVE_Z]   = std::move(parts.moveZ);
        m_parts[ROTATE_X] = std::move(parts.rotateX);
        m_parts[ROTATE_Y] = std::move(parts.rotateY);
        m_parts[ROTATE_Z] = std::move(parts.rotateZ);
        m_parts[SCALE_X]  = std::move(parts.scaleX);
        m_parts[SCALE_Y]  = std::move(parts.scaleY);
        m_parts[SCALE_Z]  = std::move(parts.scaleZ);
        m_partIds.fill(-1);
    }

    ~GizmoSystem() override = default;

    // --- Hooks do lifecycle (render thread) ---

    // Roda ANTES do beginFrame — é onde tudo que cria recurso precisa acontecer.
    // Materialização preguiçosa: enquanto initializeOverlay() falhar (ex.:
    // dependências ainda não configuradas), tenta de novo no frame seguinte.
    void onFrameBegin(float deltaTime) override {
        if (!m_initialized) {
            if (!initializeOverlay()) return;

            // O nó ao qual as 9 peças serão presas. Criado pelo renderer
            // concreto (entity/transform são específicos dele), possuído aqui.
            m_root = createRoot();

            for (size_t slot = 0; slot < SLOT_COUNT; ++slot) {
                if (!m_parts[slot].data) continue;
                // deepIds: sem ids nos nós filhos o picking devolveria -1 para
                // os meshes das peças (ver ObjectSelectorSystem::intersect).
                m_partIds[slot] = createPart(*m_parts[slot].data, m_parts[slot].materials);
            }

            m_initialized = true;
            m_pendingWiring = true;
        }

        // Ciclo da cena de overlay fora do frame: instancia o que estiver na
        // fila, roda os systems dela e faz o flush das deleções.
        updateOverlay(deltaTime);

        // Depois da instanciação as peças existem — e o parentesco já vale para
        // o desenho deste mesmo frame.
        if (m_pendingWiring) {
            for (size_t slot = 0; slot < SLOT_COUNT; ++slot) {
                if (m_partIds[slot] >= 0) attachPartToRoot(m_partIds[slot]);
            }
            m_pendingWiring = false;
        }
    }

    // Dentro do frame aberto, depois da cena 3D e antes da UI. Só desenha —
    // nada aqui pode criar recurso (ver nota sobre FEngine::prepare acima).
    void postRenderScene(float deltaTime) override {
        if (!m_initialized) return;
        renderOverlay();
    }

    // Id da peça na cena de OVERLAY (espaço de ids próprio, independente do da
    // cena principal). -1 = peça ausente ou ainda não criada.
    int getPartId(GizmoAction slot) const { return m_partIds[slot]; }

    // Nó root do gizmo — as 9 peças são presas a ele, então mexer no transform
    // dele move/rotaciona/escala o gizmo inteiro. Null antes do primeiro frame.
    NodeType* getRoot() { return m_root.get(); }

    bool isReady() const { return m_initialized; }

    // Qual eixo/modo do gizmo o raio atinge, ou nullopt se nenhum.
    // O selector varre a cena de OVERLAY, então só as 9 peças são candidatas —
    // e o hit é o de menor t (importa perto da origem, onde as peças se cruzam).
    std::optional<GizmoAction> intersectGizmo(
        glm::vec3 currentCamLocation,
        glm::vec3 clickDirection,
        float coneHalfAngle
    )
    {
        if (!m_initialized || !m_selector || !m_overlayScene) return std::nullopt;

        const int hitId = m_selector->intersect(currentCamLocation, clickDirection, coneHalfAngle);
        if (hitId < 0) return std::nullopt;

        // O id que volta é o do MESH atingido (filho, com deepIds), não o da
        // raiz criada por createPart — sobe a hierarquia até a raiz da peça.
        NodeType* node = m_overlayScene->getNode(hitId);
        if (!node) return std::nullopt;
        while (node->parent) node = node->parent;

        const int rootId = node->getId();
        for (size_t slot = 0; slot < SLOT_COUNT; ++slot) {
            if (m_partIds[slot] == rootId) return static_cast<GizmoAction>(slot);
        }

        return std::nullopt;
    }

    // Deslocamento ao longo do eixo indicado, em unidades de MUNDO.
    //
    // Algoritmo: ponto mais próximo entre duas retas (skew lines) — a reta do
    // raio da câmera e a reta INFINITA do eixo passando por objectPosition. É o
    // padrão para handle de eixo: não exige escolher um plano de arrasto e o
    // objeto acompanha o cursor naturalmente (o raio diverge com a distância,
    // então longe da câmera o mesmo pixel vale mais mundo — sem fator extra).
    //
    // O valor é ASSINADO e ABSOLUTO em relação a objectPosition: é o quanto o
    // objeto andaria para que sua origem ficasse no ponto do eixo mais próximo
    // do cursor. Para o objeto não "pular" para o cursor no primeiro frame, o
    // chamador guarda esse valor no mouse-down (grab offset) e aplica a
    // diferença: pos = pos0 + eixo * (atual - inicial).
    //
    // axis é a direção do eixo em mundo — use axisOf(action) para a ação atual.
    // axis e rayDirection podem vir com qualquer comprimento (getCameraRay
    // devolve escalado): ambos são normalizados aqui. Retorna 0 quando não há o
    // que calcular: eixo ou raio nulos, ou câmera olhando quase ao longo do
    // eixo (a solução explode; editores ignoram o update nesse caso, e o handle
    // está quase invisível de qualquer forma).
    float dragDistanceOnAxis(
        const glm::vec3& axisDirection,
        const glm::vec3& cameraPosition,
        const glm::vec3& rayDirection,
        const glm::vec3& objectPosition
    ) const {
        constexpr float eps = 1e-8f;
        constexpr float parallelEps = 1e-4f;   // |sin(ângulo raio×eixo)|² mínimo

        const float axisLength = glm::length(axisDirection);
        if (axisLength < eps) return 0.0f;
        const glm::vec3 axis = axisDirection / axisLength;

        const float rayLength = glm::length(rayDirection);
        if (rayLength < eps) return 0.0f;
        const glm::vec3 ray = rayDirection / rayLength;

        // w0 = origem do raio relativa à origem do eixo
        const glm::vec3 w0 = cameraPosition - objectPosition;
        const float b = glm::dot(ray, axis);
        const float d = glm::dot(ray, w0);
        const float e = glm::dot(axis, w0);

        // denom = 1 - cos² = sin² do ângulo entre o raio e o eixo
        const float denom = 1.0f - b * b;
        if (std::abs(denom) < parallelEps) return 0.0f;

        return (e - b * d) / denom;
    }

    TransformType* getGizmoTransform(){
        return m_root.get()->getTransform();
    }

protected:
    // Instancia e configura o selector do gizmo. Chamar do initializeOverlay()
    // da classe concreta, quando a cena de overlay já existe.
    void attachSelector(SceneType* overlayScene, CameraAsset3dInstance<TransformType>* camera) {
        m_overlayScene = overlayScene;
        m_selector = std::make_unique<SelectorType>();
        m_selector->attachTo(overlayScene);
        m_selector->setCamera(camera);
    }

    // --- Contrato do renderer concreto: TUDO roda na render thread ---

    // Cria cena/view/factory do overlay. false = tentar de novo depois.
    virtual bool initializeOverlay() = 0;

    // Cria o nó root do gizmo (entity + transform são específicos do renderer)
    // e ENTREGA A POSSE para a base. Chamado uma única vez, logo após o
    // initializeOverlay() bem-sucedido. Não passa pela cena de overlay de
    // propósito: a cena seria dona dele, e o root precisa ser do sistema.
    virtual std::unique_ptr<NodeType> createRoot() = 0;

    // Enfileira a criação de uma peça na cena de overlay; retorna o id.
    virtual int createPart(const Asset3dData& data,
                           const std::vector<MaterialData>& materials) = 0;

    // Roda o ciclo da cena de overlay (instanciar, systems, flush) FORA do frame.
    virtual void updateOverlay(float deltaTime) = 0;

    // Desenha a cena de overlay, DENTRO do frame já aberto pela cena principal.
    virtual void renderOverlay() = 0;

    // Prende a peça já instanciada ao root do gizmo.
    virtual void attachPartToRoot(int partId) = 0;

    std::array<GizmoPart, SLOT_COUNT> m_parts;
    std::array<int, SLOT_COUNT> m_partIds;
    bool m_initialized = false;
    bool m_pendingWiring = false;

    // Root do gizmo: criado pela classe concreta (createRoot), possuído aqui.
    std::unique_ptr<NodeType> m_root;

    // Picking dos eixos: vive aqui (e não na classe concreta) porque o selector
    // é agnóstico — só precisa dos dois tipos do template.
    std::unique_ptr<SelectorType> m_selector;
    SceneType* m_overlayScene = nullptr;
};

} // namespace lite
