#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/TransformManager.h>

#include <cstring>

namespace lite {

FilamentAsset3dInstance::FilamentAsset3dInstance(filament::Engine* engine, filament::Scene* scene)
    : m_engine(engine)
    , m_scene(scene)
{
}

void FilamentAsset3dInstance::initializeTransform(utils::Entity entity) {
    m_entity = entity;
    auto& tm = m_engine->getTransformManager();
    m_transform = std::make_unique<FilamentAsset3dTransform>(tm, entity);
}

void FilamentAsset3dInstance::setVisible(bool visible) {
    m_visible = visible;

    // Propagate visibility to all children recursively
    for (auto& child : children) {
        child->setVisible(visible);
    }
}

} // namespace lite
