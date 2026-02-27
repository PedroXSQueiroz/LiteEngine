#pragma once

#include <memory>
#include <vector>
#include <concepts>

#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MaterialData.h>
#include <core/data/assets/Asset3dInstance.h>

namespace lite {

// Abstract interface for instantiating 3D assets to GPU
// Implementations: FilamentInstanceFactory, etc.
template <typename A>
concept Asset3dType =
    requires { typename A::TransformType; } &&
    std::derived_from<A, Asset3dInstance<typename A::TransformType>>;

template<Asset3dType Asset>
class Asset3dInstanceFactory {
public:
    virtual ~Asset3dInstanceFactory() = default;

    // Create GPU resources from node tree and materials
    virtual std::unique_ptr<Asset> instantiate(
        const Asset3dData& rootNode,
        const std::vector<MaterialData>& materials
    ) = 0;

    // Destroy an instance and release GPU resources
    virtual void destroy(Asset* instance) = 0;
};

} // namespace lite
