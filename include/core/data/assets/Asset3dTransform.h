#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>



namespace lite {

// Abstract transform interface - renderer-agnostic
class Asset3dTransform {
public:
    virtual ~Asset3dTransform() = default;

    // Position
    virtual void setPosition(const glm::vec3& position, bool isWorldSpace = false) = 0;
    virtual glm::vec3 getPosition(bool isWorldSpace = false) = 0;

    // Rotation (quaternion)
    virtual void setRotation(const glm::quat& rotation, bool isWorldSpace = false) = 0;
    virtual glm::quat getRotation(bool isWorldSpace = false) = 0;

    // Scale
    virtual void setScale(const glm::vec3& scale, bool isWorldSpace = false) = 0;
    virtual glm::vec3 getScale(bool isWorldSpace = false) = 0;

    // Convenience: Euler angles (degrees)
    virtual void setEulerAngles(const glm::vec3& eulerDegrees, bool isWorldSpace = false);
    virtual glm::vec3 getEulerAngles(bool isWorldSpace = false);

    // Full transform matrix
    virtual void setLocalMatrix(const glm::mat4& matrix) = 0;
    virtual glm::mat4 getLocalMatrix() = 0;
    virtual glm::mat4 getWorldMatrix() = 0;

};

} // namespace lite