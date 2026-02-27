#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/TransformManager.h>

#include <cstring>

namespace lite {

void FilamentAsset3dInstance::setVisible(bool visible) {
    m_visible = visible;

    // Propagate visibility to all children recursively
    for (auto& child : children) {
        child->setVisible(visible);
    }
}

} // namespace lite
