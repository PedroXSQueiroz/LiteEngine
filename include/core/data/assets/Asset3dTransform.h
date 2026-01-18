#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lite {

// Abstract transform interface - renderer-agnostic
class Asset3dTransform {
public:
    virtual ~Asset3dTransform() = default;

    // Position
    virtual void setPosition(const glm::vec3& position) = 0;
    virtual glm::vec3 getPosition() const = 0;

    // Rotation (quaternion)
    virtual void setRotation(const glm::quat& rotation) = 0;
    virtual glm::quat getRotation() const = 0;

    // Scale
    virtual void setScale(const glm::vec3& scale) = 0;
    virtual glm::vec3 getScale() const = 0;

    // Convenience: Euler angles (degrees)
    virtual void setEulerAngles(const glm::vec3& eulerDegrees);
    virtual glm::vec3 getEulerAngles() const;

    // Full transform matrix
    virtual void setLocalMatrix(const glm::mat4& matrix) = 0;
    virtual glm::mat4 getLocalMatrix() const = 0;
    virtual glm::mat4 getWorldMatrix() const = 0;
};

// Default implementations for Euler conversion
inline void Asset3dTransform::setEulerAngles(const glm::vec3& eulerDegrees) {
    setRotation(glm::quat(glm::radians(eulerDegrees)));
}

inline glm::vec3 Asset3dTransform::getEulerAngles() const {
    return glm::degrees(glm::eulerAngles(getRotation()));
}

} // namespace lite