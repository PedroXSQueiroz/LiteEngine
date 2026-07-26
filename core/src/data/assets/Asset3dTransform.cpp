#include <core/data/assets/Asset3dTransform.h>

#include <glm/gtc/quaternion.hpp>

namespace lite {

void Asset3dTransform::setEulerAngles(const glm::vec3& eulerDegrees, bool isWorldSpace) {
    setRotation(glm::quat(glm::radians(eulerDegrees)), isWorldSpace);
}

glm::vec3 Asset3dTransform::getEulerAngles(bool isWorldSpace) {
    return glm::degrees(glm::eulerAngles(getRotation(isWorldSpace)));
}

} // namespace lite
