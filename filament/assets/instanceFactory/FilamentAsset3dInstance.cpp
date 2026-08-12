#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/TransformManager.h>

#include <cstring>

namespace lite {

void FilamentAsset3dInstance::setVisible(bool visible) {
    m_visible = visible;

    // Propagate visibility to all children recursively
    // Os elos da árvore são Node; setVisible só existe no Asset3dInstance.
    for (auto& child : children) {
        if (auto* instance = dynamic_cast<Asset3dInstance<FilamentAsset3dTransform>*>(child.get())) {
            instance->setVisible(visible);
        }
    }
}

} // namespace lite
