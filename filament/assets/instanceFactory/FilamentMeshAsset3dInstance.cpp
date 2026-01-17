#include <filament/data/assets/FilamentMeshAsset3dInstance.h>

#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>

namespace lite {

FilamentMeshAsset3dInstance::FilamentMeshAsset3dInstance(filament::Engine* engine, filament::Scene* scene)
    : m_engine(engine)
    , m_scene(scene)
{
}

void FilamentMeshAsset3dInstance::initializeTransform(utils::Entity entity) {
    m_entity = entity;
    auto& tm = m_engine->getTransformManager();
    m_transform = std::make_unique<FilamentAsset3dTransform>(tm, entity);
}

void FilamentMeshAsset3dInstance::setVisible(bool visible) {
    m_visible = visible;

    auto& renderableManager = m_engine->getRenderableManager();
    auto instance = renderableManager.getInstance(m_entity);
    if (instance) {
        // Use setLayerMask to show/hide
        // Layer 0 is typically the main camera layer
        renderableManager.setLayerMask(instance, 0xFF, visible ? 0xFF : 0x00);
    }
}

} // namespace lite
