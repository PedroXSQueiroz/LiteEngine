#pragma once

#include <core/data/assets/Asset3dInstance.h>

#include <string>

namespace lite {

// Mesh-specific instance (holds GPU resources for a single mesh)
template<TransformConcept Transform>
class MeshAsset3dInstance : public Asset3dInstance<Transform> {
public:
    ~MeshAsset3dInstance() override = default;

    bool isMesh() const override { return true; }

    // Material name reference (for lookup)
    std::string materialName;
};

} // namespace lite
