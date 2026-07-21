#pragma once

#include <core/SceneScopeSystem.h>
#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MaterialData.h>

#include <array>
#include <memory>
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

// Sistema do gizmo de transformação — renderer agnostic.
//
// Ao contrário dos outros systems, este é DONO DE UMA CENA: o gizmo é desenhado
// num overlay próprio (cena + view separadas da cena 3D), composto por cima dela
// dentro do mesmo frame. Isso mantém o gizmo sempre visível (não é ocluído pela
// geometria) e fora do picking da cena principal. A base cuida do ciclo; o
// renderer concreto (ex.: FilamentGizmoSystem) fornece as 4 operações virtuais.
//
// Ciclo (ambos os hooks executam NA RENDER THREAD):
//   onFrameBegin     → FORA do frame GPU: 1ª vez initializeOverlay() +
//                      createPart() × 9; todo frame updateOverlay(dt)
//                      (instancia o que está na fila); 1ª vez attachPartToRoot()
//   postRenderScene  → DENTRO do frame: renderOverlay()
//
// A separação instanciar-fora / desenhar-dentro é OBRIGATÓRIA, não estética:
// FEngine::prepare() roda uma vez por frame dentro do beginFrame() e é ele quem
// faz o commit de todas as MaterialInstance. Uma instância criada depois do
// beginFrame não passa por esse commit, e o descriptor set dela chega ao render
// pass sem handle → assert 'mDescriptorSetHandle' no Filament. Por isso nada
// que crie recurso pode rodar no postRenderScene.
class GizmoSystem : public SceneScopeSystem {
public:
    enum Slot : size_t {
        MOVE_X = 0, MOVE_Y, MOVE_Z,
        ROTATE_X, ROTATE_Y, ROTATE_Z,
        SCALE_X, SCALE_Y, SCALE_Z,
        SLOT_COUNT
    };

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
    int getPartId(Slot slot) const { return m_partIds[slot]; }

    bool isReady() const { return m_initialized; }

protected:
    // --- Contrato do renderer concreto: TUDO roda na render thread ---

    // Cria cena/view/factory do overlay e o root. false = tentar de novo depois.
    virtual bool initializeOverlay() = 0;

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
};

} // namespace lite
