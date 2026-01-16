#pragma once

#include <core/data/assets/Asset3dInstance.h>

#include <string>

namespace lite {

// Mesh-specific instance (holds GPU resources for a single mesh)
class MeshAsset3dInstance : public Asset3dInstance {
public:
    ~MeshAsset3dInstance() override = default;

    bool isMesh() const override { return true; }

    // Material name reference (for lookup)
    std::string materialName;
};

} // namespace lite
