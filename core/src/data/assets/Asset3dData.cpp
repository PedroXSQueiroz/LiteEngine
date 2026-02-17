#include <core/data/assets/Asset3dData.h>

namespace lite {

glm::mat4 Asset3dData::getWorldTransform() const {
    if (!parent) {
        return localTransform;
    }
    return parent->getWorldTransform() * localTransform;
}

} // namespace lite
