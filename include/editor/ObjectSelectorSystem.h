#pragma once

#include <core/SceneScopeSystem.h>
#include <core/data/assets/Asset3dInstance.h>
#include <core/data/assets/MeshAsset3dInstance.h>
#include <core/data/assets/CameraAsset3dInstance.h>

#include <glm/glm.hpp>

#include <vector>
#include <cmath>
#include <utility>
#include <optional>
#include <limits>
#include <set>
#include <iostream>
#include <format>

namespace lite {

// Sistema de seleção de objetos por raio (picking) — renderer agnostic.
// Mesmo molde do WireframeSystem: deriva de SceneScopeSystem e é ligado à cena
// via attachTo(). Não implementa nenhum hook hoje (é um serviço passivo de
// consulta); derivar de SceneScopeSystem o mantém plugável no ciclo do frame
// (ex.: processar uma fila de cliques por frame no futuro).
//
// SceneType:     a cena concreta (ex.: FilamentScene) — mesmo idioma do
//                SceneRenderer<SceneType>; necessário porque Scene é template
//                sem base comum.
// TransformType: o transform concreto dos nós. A geometria vem dos contratos
//                virtuais da classe PAI MeshAsset3dInstance<TransformType>
//                (getVertex/getIndex/getBoundingBox) — nenhum tipo concreto de
//                mesh é referenciado; qualquer subclasse é selecionável.
//
// Uso (ou via subclasse que fixa os parâmetros, ex.: FilamentObjectSelectorSystem):
//   selector = make_unique<FilamentObjectSelectorSystem>();
//   selector->attachTo(liteScene);
//   selector->setCamera(cameraInstance);          // CameraAsset3dInstance da câmera
//   glm::vec3 origin = selector->getCameraPosition();
//   glm::vec3 ray    = selector->getCameraRay({mouseX, mouseY}, {width, height}, 100.0f);
//   int id = selector->intersect(origin, ray, coneHalfAngle);  // -1 = nada atingido
//
// THREADING: intersect() usa Scene::find() (lock interno) e só enxerga
// instâncias já instanciadas — seguro de qualquer thread. getBoundingBox() é
// lazy (pode calcular/armazenar na 1ª chamada); evitar chamadas concorrentes
// ao mesmo mesh vindas de threads diferentes.
//
// LIMITAÇÕES: retorna o PRIMEIRO mesh atingido na ordem de percurso (não o
// mais próximo da câmera); nós filhos criados sem deepIds têm id -1.
template <typename SceneType, TransformConcept TransformType>
class ObjectSelectorSystem : public SceneScopeSystem {
public:
    using NodeType = Asset3dInstance<TransformType>;
    using MeshNodeType = MeshAsset3dInstance<TransformType>;

    virtual ~ObjectSelectorSystem() = default;

    // Liga o sistema à cena que ele consulta
    void attachTo(SceneType* scene) { m_scene = scene; }

    // Nó de cena da câmera — posição/view vêm da world matrix do nó (nunca da
    // view do renderer); só a projeção usa o contrato CameraAsset3dInstance.
    void setCamera(CameraAsset3dInstance<TransformType>* camera) { m_camera = camera; }

    glm::vec3 getCameraPosition() const {
        if (!m_camera) return glm::vec3(0.0f);
        return glm::vec3(m_camera->getWorldMatrix()[3]);
    }

    // origin: início do segmento (mundo); ray: direção JÁ escalada pelo
    // alcance (mundo) — o teste cobre apenas o segmento [origin, origin+ray].
    // coneHalfAngle: meio-ângulo (radianos) do cone que envolve a direção do
    // raio; a broad phase mantém só objetos cuja esfera envolvente cai DENTRO
    // desse cone (direcional) e ao alcance do segmento. Calcule-o a partir da
    // abertura (FOV) da câmera no chamador.
    // Retorna o id do primeiro mesh cuja geometria é atingida, ou -1.
    int intersect(const glm::vec3& origin, const glm::vec3& ray, float coneHalfAngle) {
        if (!m_scene) return -1;

        const float rayLength = glm::length(ray);
        if (rayLength < 1e-8f) return -1;
        const glm::vec3 rayDir = ray / rayLength;

        // Instâncias deletadas permanecem no mapa da Scene (deleção em duas
        // fases) — filtra quando o tipo concreto expõe isDeleted().
        //FIXME: é possível usar Asset3dInstance ao invés de auto?
        auto roots = m_scene->find([origin, rayDir, rayLength, coneHalfAngle](auto* node) {
            if (node == nullptr) return false;
            if constexpr (requires { node->isDeleted(); }) {
                if (node->isDeleted()) return false;
            }

            // Esfera envolvente (mundo) de todos os meshes descendentes. Usa o
            // RAIO do bounding box, não o pivô — asset cujo pivô está longe mas
            // a malha perto não é descartado.
            glm::vec3 mn, mx;
            bool hasGeometry = false;
            accumulateWorldBounds(*node, mn, mx, hasGeometry);
            if (!hasGeometry) return true; // nó sem mesh: nada a cular → mantém

            const glm::vec3 center = 0.5f * (mn + mx);
            const float radius = 0.5f * glm::length(mx - mn);
            const glm::vec3 toCenter = center - origin;
            const float dist = glm::length(toCenter);

            // Alcance: esfera além do fim do segmento → descarta.
            if (dist > rayLength + radius) return false;
            // Origem dentro da esfera → mantém (ângulo indefinido).
            if (dist <= radius) return true;

            // Direcional: o ângulo do centro à direção do raio, descontado o
            // meio-ângulo subentendido pela esfera (asin(r/dist)), tem de caber
            // no cone.
            const float alpha = std::acos(glm::clamp(glm::dot(toCenter / dist, rayDir), -1.0f, 1.0f));
            const float beta = std::asin(radius / dist);
            return (alpha - beta) <= coneHalfAngle;
        });

        // A varredura em si é a sobrecarga pública abaixo. Converte para o tipo
        // BASE dos nós (só cópia de ponteiros): find() devolve AssetType*, mas
        // os meshes descendem de MeshAsset3dInstance — o ancestral comum é
        // Asset3dInstance<TransformType>.
        std::vector<NodeType*> candidates(roots.begin(), roots.end());
        return intersect(origin, ray, candidates);
    }

    // Mesma varredura do intersect() acima, porém sobre um conjunto de roots
    // JÁ escolhido pelo chamador — sem broad phase (o cone/alcance vive no
    // filtro do find(), ou seja, antes deste ponto).
    // Serve para consultar conjuntos que não estão na cena deste selector (ex.:
    // o gizmo, que vive em outra Scene): chama-se primeiro com os roots do
    // gizmo e, se voltar -1, chama-se o intersect() normal com o MESMO raio.
    // Retorna o id do mesh de menor t (mais próximo da origem), ou -1.
    int intersect(const glm::vec3& origin, const glm::vec3& ray,
                  const std::vector<NodeType*>& roots) {
        // t é comparável entre meshes: o segmento de mundo é o mesmo, apenas
        // transformado para o espaço local de cada mesh.
        int bestId = -1;
        float bestT = std::numeric_limits<float>::max();
        for (auto* root : roots) {
            if (root == nullptr) continue;
            intersectNode(*root, origin, ray, bestId, bestT);
        }
        return bestId;
    }

    void addSelected(Asset3dInstance<TransformType>* newSelected)
    {
        if( !m_selectedObjects.contains(newSelected) ) {
            m_selectedObjects.insert(newSelected);
        }
    }

    void clearSelected()
    {
        m_selectedObjects.empty();
    }

    glm::vec3 getSelectionMedianPoint()
    {
        glm::vec3 medianPoint = glm::vec3(0, 0, 0);
        
        for(Asset3dInstance<TransformType>* selected : m_selectedObjects) 
        { 
            glm::vec3 position = selected->getTransform()->getPosition(true);
            std::cout << std::format("Calculating median point => x:{}, y:{}, z:{}", position.x, position.y, position.z) << std::endl;
            medianPoint += position;
        } 

        medianPoint /= m_selectedObjects.size();

        return medianPoint;
    }

protected:
    // Expande [mn, mx] com a AABB (em mundo) de todos os meshes descendentes de
    // `node`. hasGeometry vira true na primeira contribuição — enquanto false,
    // mn/mx não têm valor válido. Broad-phase do filtro por distância no
    // intersect(); getBoundingBox() é lazy (cacheia na 1ª chamada).
    static void accumulateWorldBounds(Node& node, glm::vec3& mn, glm::vec3& mx, bool& hasGeometry) {
        // Os elos da árvore são Node: o cast é o que identifica um mesh (o
        // isMesh() anterior era redundante com ele).
        if (auto* mesh = dynamic_cast<MeshNodeType*>(&node)) {
            const std::vector<glm::vec3> bounds = mesh->getBoundingBox();
            if (bounds.size() >= 2) {
                const glm::mat4 world = mesh->getWorldMatrix();
                // 8 cantos da AABB local → mundo (cobre rotação e escala)
                for (int i = 0; i < 8; ++i) {
                    const glm::vec3 corner(
                        (i & 1) ? bounds[1].x : bounds[0].x,
                        (i & 2) ? bounds[1].y : bounds[0].y,
                        (i & 4) ? bounds[1].z : bounds[0].z);
                    const glm::vec3 wc = glm::vec3(world * glm::vec4(corner, 1.0f));
                    if (!hasGeometry) { mn = mx = wc; hasGeometry = true; }
                    else { mn = glm::min(mn, wc); mx = glm::max(mx, wc); }
                }
            }
        }
        for (auto& child : node.children) {
            accumulateWorldBounds(*child, mn, mx, hasGeometry);
        }
    }

    // Percorre a subárvore; atualiza bestId/bestT com o hit de menor t.
    void intersectNode(Node& node, const glm::vec3& origin, const glm::vec3& ray,
                       int& bestId, float& bestT) {
        // Os elos da árvore são Node: o cast é o que identifica um mesh (o
        // isMesh() anterior era redundante com ele).
        if (auto* mesh = dynamic_cast<MeshNodeType*>(&node)) {
            if (std::optional<float> t = meshHit(*mesh, origin, ray); t && *t < bestT) {
                bestT = *t;
                bestId = mesh->getId();
            }
        }
        for (auto& child : node.children) {
            intersectNode(*child, origin, ray, bestId, bestT);
        }
    }

    // Retorna o MENOR t (∈[0,1], parâmetro ao longo do segmento de mundo) entre
    // todos os triângulos atingidos, ou nullopt se o mesh não é atingido.
    std::optional<float> meshHit(MeshNodeType& mesh, const glm::vec3& worldOrigin, const glm::vec3& worldRay) {
        // Geometria e bounding box são locais ao mesh: leva o segmento para o
        // espaço local (direção NÃO normalizada — t em [0,1] cobre o segmento
        // inteiro em qualquer espaço, mesmo com escala não uniforme).
        const glm::mat4 invWorld = glm::inverse(mesh.getWorldMatrix());
        const glm::vec3 o = glm::vec3(invWorld * glm::vec4(worldOrigin, 1.0f));
        const glm::vec3 d = glm::vec3(invWorld * glm::vec4(worldRay, 0.0f));

        const std::vector<glm::vec3> bounds = mesh.getBoundingBox();
        if (bounds.size() < 2 || !segmentIntersectsAabb(o, d, bounds[0], bounds[1])) {
            return std::nullopt;
        }

        const std::vector<glm::vec3> vertices = mesh.getVertex();
        const std::vector<int64_t> indices = mesh.getIndex();
        std::optional<float> best;
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const glm::vec3& a = vertices[static_cast<size_t>(indices[i])];
            const glm::vec3& b = vertices[static_cast<size_t>(indices[i + 1])];
            const glm::vec3& c = vertices[static_cast<size_t>(indices[i + 2])];
            float t;
            if (mollerTrumbore(o, d, a, b, c, t) && t >= 0.0f && t <= 1.0f && (!best || t < *best)) {
                best = t;
            }
        }
        return best;
    }

    // Slab test do segmento o + t*d, t em [0,1], contra a AABB [mn, mx]
    static bool segmentIntersectsAabb(const glm::vec3& o, const glm::vec3& d,
                                      const glm::vec3& mn, const glm::vec3& mx) {
        float tMin = 0.0f;
        float tMax = 1.0f;
        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(d[axis]) < 1e-8f) {
                if (o[axis] < mn[axis] || o[axis] > mx[axis]) return false;
            } else {
                float inv = 1.0f / d[axis];
                float t0 = (mn[axis] - o[axis]) * inv;
                float t1 = (mx[axis] - o[axis]) * inv;
                if (t0 > t1) std::swap(t0, t1);
                tMin = std::max(tMin, t0);
                tMax = std::min(tMax, t1);
                if (tMin > tMax) return false;
            }
        }
        return true;
    }

    // Möller–Trumbore sem backface culling; t sai na unidade do vetor d
    static bool mollerTrumbore(const glm::vec3& o, const glm::vec3& d,
                               const glm::vec3& a, const glm::vec3& b,
                               const glm::vec3& c, float& t) {
        constexpr float eps = 1e-8f;
        const glm::vec3 e1 = b - a;
        const glm::vec3 e2 = c - a;
        const glm::vec3 p = glm::cross(d, e2);
        const float det = glm::dot(e1, p);
        if (std::abs(det) < eps) return false;
        const float invDet = 1.0f / det;
        const glm::vec3 s = o - a;
        const float u = glm::dot(s, p) * invDet;
        if (u < 0.0f || u > 1.0f) return false;
        const glm::vec3 q = glm::cross(s, e1);
        const float v = glm::dot(d, q) * invDet;
        if (v < 0.0f || u + v > 1.0f) return false;
        t = glm::dot(e2, q) * invDet;
        return true;
    }

    SceneType* m_scene = nullptr;
    CameraAsset3dInstance<TransformType>* m_camera = nullptr;
    std::set<Asset3dInstance<TransformType>*> m_selectedObjects = {};
};

} // namespace lite
