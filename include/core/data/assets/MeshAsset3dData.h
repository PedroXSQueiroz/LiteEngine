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

    std::unique_ptr<Asset3dData> clone() const override {
        auto copy = std::make_unique<MeshAsset3dData>();
        copy->name = name;
        copy->localTransform = localTransform;
        for (const auto& child : children) {
            auto childClone = child->clone();
            childClone->parent = copy.get();
            copy->children.push_back(std::move(childClone));
        }
        copy->positions = positions;
        copy->normals    = normals;
        copy->uvs        = uvs;
        copy->indices    = indices;
        copy->boundsMin  = boundsMin;
        copy->boundsMax  = boundsMax;
        copy->center     = center;
        copy->radius     = radius;
        copy->materialName = materialName;
        return copy;
    }
};

} // namespace lite
