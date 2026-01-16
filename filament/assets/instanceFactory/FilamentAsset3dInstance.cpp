#include <filament/data/assets/FilamentAsset3dInstance.h>

#include <cstring>

namespace lite {

void FilamentAsset3dInstance::setTransform(const glm::mat4& transform) {
    m_transform = transform;
    localTransform = transform;
    // Note: Individual mesh transforms are handled by FilamentMeshAsset3dInstance
    // The localTransform is used for getWorldTransform() calculations
}

void FilamentAsset3dInstance::setVisible(bool visible) {
    m_visible = visible;

    // Propagate visibility to all children recursively
    for (auto& child : children) {
        child->setVisible(visible);
    }
}

} // namespace lite
