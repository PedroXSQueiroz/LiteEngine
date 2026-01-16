#include <filament/assets/instanceFactory/FilamentMeshAsset3dInstance.h>

#include <filament/RenderableManager.h>

namespace lite {

void FilamentMeshAsset3dInstance::setVisible(bool visible) {
    m_visible = visible;

    auto& renderableManager = m_engine->getRenderableManager();
    auto instance = renderableManager.getInstance(entity);
    if (instance) {
        // Use setLayerMask to show/hide
        // Layer 0 is typically the main camera layer
        renderableManager.setLayerMask(instance, 0xFF, visible ? 0xFF : 0x00);
    }
}

} // namespace lite
