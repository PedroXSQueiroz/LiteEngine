#pragma once

#include <memory>
#include <vector>

#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MaterialData.h>
#include <core/data/assets/Asset3dInstance.h>

namespace lite {

// Abstract interface for instantiating 3D assets to GPU
// Implementations: FilamentInstanceFactory, etc.
class Asset3dInstanceFactory {
public:
    virtual ~Asset3dInstanceFactory() = default;

    // Create GPU resources from node tree and materials
    virtual std::unique_ptr<Asset3dInstance> instantiate(
        const Asset3dData& rootNode,
        const std::vector<MaterialData>& materials
    ) = 0;

    // Destroy an instance and release GPU resources
    virtual void destroy(Asset3dInstance* instance) = 0;
};

} // namespace lite
