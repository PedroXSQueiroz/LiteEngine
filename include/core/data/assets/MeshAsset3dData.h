#pragma once

#include <core/data/assets/Asset3dData.h>

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

namespace lite {

// Mesh-specific 3D asset data
class MeshAsset3dData : public Asset3dData {
public:
    // Geometry data
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;

    // Bounding box
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    glm::vec3 center = glm::vec3(0.0f);
    float radius = 0.0f;

    // Material reference by NAME (not index)
    std::string materialName;

    bool isMesh() const override { return true; }
};

} // namespace lite
