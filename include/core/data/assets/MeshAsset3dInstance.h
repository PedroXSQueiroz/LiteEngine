#pragma once

#include <core/data/assets/Asset3dInstance.h>

#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

namespace lite {

// Mesh-specific instance (holds GPU resources for a single mesh)
template<TransformConcept Transform>
class MeshAsset3dInstance : public Asset3dInstance<Transform> {
public:
    ~MeshAsset3dInstance() override = default;

    bool isMesh() const override { return true; }

    // CPU-side geometry access (storage lives in the concrete implementation)
    virtual std::vector<glm::vec3> getVertex() const = 0;
    virtual std::vector<int64_t> getIndex() const = 0;
    virtual std::vector<glm::vec2> getUVS(int index) const = 0;

    // Bounding box as {min, max}. getBoundingBox is lazy: computes via
    // calcBoundingBox on first call if no value was set; setBoundingBox overrides.
    virtual std::vector<glm::vec3> getBoundingBox() = 0;
    virtual void setBoundingBox(const std::vector<glm::vec3>& bounds) = 0;
    virtual std::vector<glm::vec3> calcBoundingBox() const = 0;

    // AABB {min, max} agregada deste nó + TODOS os meshes descendentes, no
    // espaço LOCAL deste nó (a world transform do próprio nó é excluída: cada
    // mesh entra pela sua transform RELATIVA a este nó, então o resultado
    // independe de onde/como este nó está posicionado no mundo).
    //
    // Traversal igual ao do picking (ObjectSelectorSystem): um nó sem geometria
    // não contribui com bound próprio, mas a recursão continua descendo pelos
    // filhos (netos-mesh entram normalmente).
    //
    // getBoundingBox() de cada mesh é LOCAL ao mesh; a composição para o espaço
    // deste nó usa inverse(this.world) * mesh.world (identidade para o próprio
    // nó). SEM cache: recalcula a cada chamada. Sem geometria → {0,0}.
    std::vector<glm::vec3> getCompleteBoundingBox() {
        const glm::mat4 invNodeWorld = glm::inverse(this->getWorldMatrix());
        glm::vec3 mn(0.0f), mx(0.0f);
        bool hasGeometry = false;
        accumulateLocalBounds(*this, invNodeWorld, mn, mx, hasGeometry);
        return { mn, mx };
    }

    // Material name reference (for lookup)
    std::string materialName;

private:
    // Expande [mn, mx] com a AABB de cada mesh descendente de `node`, trazida
    // para o espaço definido por invNodeWorld (relativa ao nó consultado).
    static void accumulateLocalBounds(
        Asset3dInstance<Transform>& node, const glm::mat4& invNodeWorld,
        glm::vec3& mn, glm::vec3& mx, bool& hasGeometry
    ) {
        if (node.isMesh()) {
            if (auto* mesh = dynamic_cast<MeshAsset3dInstance<Transform>*>(&node)) {
                const std::vector<glm::vec3> bounds = mesh->getBoundingBox();
                if (bounds.size() >= 2) {
                    // transform do mesh RELATIVA ao nó consultado (identidade
                    // quando node == this: inverse(W)*W)
                    const glm::mat4 rel = invNodeWorld * mesh->getWorldMatrix();
                    // 8 cantos da AABB local do mesh → espaço do nó consultado
                    for (int i = 0; i < 8; ++i) {
                        const glm::vec3 corner(
                            (i & 1) ? bounds[1].x : bounds[0].x,
                            (i & 2) ? bounds[1].y : bounds[0].y,
                            (i & 4) ? bounds[1].z : bounds[0].z);
                        const glm::vec3 lc = glm::vec3(rel * glm::vec4(corner, 1.0f));
                        if (!hasGeometry) { mn = mx = lc; hasGeometry = true; }
                        else { mn = glm::min(mn, lc); mx = glm::max(mx, lc); }
                    }
                }
            }
        }
        for (auto& child : node.children) {
            accumulateLocalBounds(*child, invNodeWorld, mn, mx, hasGeometry);
        }
    }
};

} // namespace lite
