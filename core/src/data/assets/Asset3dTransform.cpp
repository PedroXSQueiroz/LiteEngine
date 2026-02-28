#include <core/data/assets/Asset3dTransform.h>

#include <glm/gtc/quaternion.hpp>

namespace lite {

void Asset3dTransform::setEulerAngles(const glm::vec3& eulerDegrees) {
    setRotation(glm::quat(glm::radians(eulerDegrees)));
}

glm::vec3 Asset3dTransform::getEulerAngles() {
    return glm::degrees(glm::eulerAngles(getRotation()));
}

} // namespace lite
