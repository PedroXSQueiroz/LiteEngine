#pragma once

#include <glm/glm.hpp>

namespace lite {

    // Abstract interface for instantiated 3D assets (GPU resources)
    // Implementations: FilamentAsset3dInstance, etc.
    class Asset3dInstance {
    public:
        virtual ~Asset3dInstance() = default;

        // Set the world transform of the entire asset
        virtual void setTransform(const glm::mat4& transform) = 0;

        // Set visibility
        virtual void setVisible(bool visible) = 0;

        // Get current transform
        virtual glm::mat4 getTransform() const = 0;

        // Check if visible
        virtual bool isVisible() const = 0;
    };

} // namespace lite
