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
    
    // Material name reference (for lookup)
    std::string materialName;
};

} // namespace lite
