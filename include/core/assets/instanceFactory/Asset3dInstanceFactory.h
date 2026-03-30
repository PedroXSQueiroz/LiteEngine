#pragma once

#include <memory>
#include <vector>

#include <core/concepts/Asset3dConcept.h>
#include <core/concepts/TransformConcept.h>
#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MaterialData.h>

namespace lite {

// Abstract interface for instantiating 3D assets to GPU
// Implementations: FilamentInstanceFactory, etc.
template<Asset3dConcept Asset, TransformConcept Transform>
class Asset3dInstanceFactory {
public:
    
using AssetType = Asset;
using TransformType = Transform;

    virtual ~Asset3dInstanceFactory() = default;

    // Create GPU resources from node tree and materials
    virtual std::unique_ptr<Asset> instantiateAsset(
        const Asset3dData& rootNode,
        Transform transform,
        const std::vector<MaterialData>& materials
    ) = 0;

    // Destroy an instance and release GPU resources
    virtual bool destroyAsset(Asset* instance) = 0;
};

} // namespace lite
