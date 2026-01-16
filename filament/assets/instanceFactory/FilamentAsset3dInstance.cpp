#include <filament/assets/instanceFactory/FilamentAsset3dInstance.h>

#include <filament/TransformManager.h>
#include <filament/RenderableManager.h>

#include <cstring>

namespace lite {

void FilamentAsset3dInstance::setTransform(const glm::mat4& transform) {
    m_transform = transform;

    // Convert GLM to Filament matrix
    filament::math::mat4f filamentTransform;
    std::memcpy(&filamentTransform, &transform, sizeof(float) * 16);

    // Apply to root entity
    auto& transformManager = m_engine->getTransformManager();
    auto instance = transformManager.getInstance(rootEntity);
    if (instance) {
        transformManager.setTransform(instance, filamentTransform);
    }
}

void FilamentAsset3dInstance::setVisible(bool visible) {
    m_visible = visible;

    auto& renderableManager = m_engine->getRenderableManager();

    for (auto& entity : entities) {
        auto instance = renderableManager.getInstance(entity);
        if (instance) {
            // Use setLayerMask to show/hide
            // Layer 0 is typically the main camera layer
            renderableManager.setLayerMask(instance, 0xFF, visible ? 0xFF : 0x00);
        }
    }
}

} // namespace lite
