#include <core/data/assets/Asset3dInstance.h>

namespace lite {

glm::mat4 Asset3dInstance::getLocalMatrix() const {
    return m_transform ? m_transform->getLocalMatrix() : glm::mat4(1.0f);
}

glm::mat4 Asset3dInstance::getWorldMatrix() const {
    return m_transform ? m_transform->getWorldMatrix() : glm::mat4(1.0f);
}

void Asset3dInstance::setLocalMatrix(const glm::mat4& matrix) {
    if (m_transform) m_transform->setLocalMatrix(matrix);
}

} // namespace lite
