#pragma once

#include <memory>
#include <lite/core/Asset3dImportData.h>
#include <lite/core/Asset3dInstance.h>

namespace lite {

    // Abstract interface for instantiating 3D assets to GPU
    // Implementations: FilamentInstantiator, etc.
    class Asset3dInstantiator {
    public:
        virtual ~Asset3dInstantiator() = default;

        // Create GPU resources from imported data
        virtual std::unique_ptr<Asset3dInstance> instantiate(const Asset3dImportData& data) = 0;

        // Destroy an instance and release GPU resources
        virtual void destroy(Asset3dInstance* instance) = 0;
    };

} // namespace lite
