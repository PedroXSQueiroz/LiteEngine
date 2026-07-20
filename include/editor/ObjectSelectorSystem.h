#pragma once

#include <core/SceneScopeSystem.h>
#include <core/data/assets/Asset3dInstance.h>
#include <core/data/assets/MeshAsset3dInstance.h>
#include <core/data/assets/CameraAsset3dInstance.h>

#include <glm/glm.hpp>

#include <vector>
#include <cmath>
#include <utility>

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
//   int id = selector->intersect(origin, ray);    // -1 = nada atingido
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

    // Vetor saindo da câmera em direção ao pixel clicado, com o comprimento
    // pedido. Inverte o pipeline de transformação: viewport → NDC → clip →
    // view → mundo. pixel em coordenadas de janela (origem no canto superior
    // esquerdo, como o SDL entrega o mouse).
    glm::vec3 getCameraRay(const glm::vec2& pixel, const glm::vec2& viewportSize, float length) const {
        if (!m_camera || viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
            return glm::vec3(0.0f);
        }

        // viewport → NDC: y da tela cresce para baixo, NDC cresce para cima
        const float x = (2.0f * pixel.x) / viewportSize.x - 1.0f;
        const float y = 1.0f - (2.0f * pixel.y) / viewportSize.y;

        const glm::mat4 world = m_camera->getWorldMatrix();
        const glm::mat4 view = glm::inverse(world); // localização do nó, não a view do renderer
        const glm::mat4 invVP = glm::inverse(m_camera->getProjectionMatrix() * view);

        // clip → mundo: dois depths distintos do mesmo pixel caem sobre a
        // mesma reta no mundo, independente da convenção de z da projeção
        // (NO [-1,1] ou ZO [0,1]) — dispensa acertar o valor do near plane
        glm::vec4 p0 = invVP * glm::vec4(x, y, 0.0f, 1.0f);
        glm::vec4 p1 = invVP * glm::vec4(x, y, 0.5f, 1.0f);
        constexpr float eps = 1e-8f;
        if (std::abs(p0.w) < eps || std::abs(p1.w) < eps) return glm::vec3(0.0f);

        const glm::vec3 a = glm::vec3(p0) / p0.w;
        const glm::vec3 b = glm::vec3(p1) / p1.w;
        glm::vec3 dir = b - a;
        if (glm::dot(dir, dir) < eps) return glm::vec3(0.0f);
        dir = glm::normalize(dir);

        // orienta para frente da câmera (o par de depths pode sair invertido
        // conforme a convenção de projeção)
        const glm::vec3 forward = -glm::normalize(glm::vec3(world[2]));
        if (glm::dot(dir, forward) < 0.0f) dir = -dir;

        return dir * length;
    }

    // origin: início do segmento (mundo); ray: direção JÁ escalada pelo
    // alcance (mundo) — o teste cobre apenas o segmento [origin, origin+ray].
    // Retorna o id do primeiro mesh cuja geometria é atingida, ou -1.
    int intersect(const glm::vec3& origin, const glm::vec3& ray) {
        if (!m_scene) return -1;

        // Instâncias deletadas permanecem no mapa da Scene (deleção em duas
        // fases) — filtra quando o tipo concreto expõe isDeleted().
        auto roots = m_scene->find([](auto* node) {
            if constexpr (requires { node->isDeleted(); }) {
                return node != nullptr && !node->isDeleted();
            } else {
                return node != nullptr;
            }
        });

        for (auto* root : roots) {
            int id = intersectNode(*root, origin, ray);
            if (id != -1) return id;
        }
        return -1;
    }

protected:
    int intersectNode(NodeType& node, const glm::vec3& origin, const glm::vec3& ray) {
        if (node.isMesh()) {
            if (auto* mesh = dynamic_cast<MeshNodeType*>(&node)) {
                if (meshHit(*mesh, origin, ray)) return mesh->getId();
            }
        }
        for (auto& child : node.children) {
            int id = intersectNode(*child, origin, ray);
            if (id != -1) return id;
        }
        return -1;
    }

    bool meshHit(MeshNodeType& mesh, const glm::vec3& worldOrigin, const glm::vec3& worldRay) {
        // Geometria e bounding box são locais ao mesh: leva o segmento para o
        // espaço local (direção NÃO normalizada — t em [0,1] cobre o segmento
        // inteiro em qualquer espaço, mesmo com escala não uniforme).
        const glm::mat4 invWorld = glm::inverse(mesh.getWorldMatrix());
        const glm::vec3 o = glm::vec3(invWorld * glm::vec4(worldOrigin, 1.0f));
        const glm::vec3 d = glm::vec3(invWorld * glm::vec4(worldRay, 0.0f));

        const std::vector<glm::vec3> bounds = mesh.getBoundingBox();
        if (bounds.size() < 2 || !segmentIntersectsAabb(o, d, bounds[0], bounds[1])) {
            return false;
        }

        const std::vector<glm::vec3> vertices = mesh.getVertex();
        const std::vector<int64_t> indices = mesh.getIndex();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const glm::vec3& a = vertices[static_cast<size_t>(indices[i])];
            const glm::vec3& b = vertices[static_cast<size_t>(indices[i + 1])];
            const glm::vec3& c = vertices[static_cast<size_t>(indices[i + 2])];
            float t;
            if (mollerTrumbore(o, d, a, b, c, t) && t >= 0.0f && t <= 1.0f) {
                return true;
            }
        }
        return false;
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
};

} // namespace lite
