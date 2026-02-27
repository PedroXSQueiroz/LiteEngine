#pragma once

#include <core/data/assets/Asset3dInstance.h>
#include <glm/glm.hpp>

namespace lite {

// template <typename T>
// concept TransformType = std::derived_from<T, Asset3dTransform>;

// Abstract camera that is also a scene node
template<TransformClass Transform>
class CameraAsset3dInstance : public Asset3dInstance<Transform> {
public:
    virtual ~CameraAsset3dInstance() = default;

    // View matrix (computed from lookAt or transform)
    virtual glm::mat4 getViewMatrix() const = 0;

    // Projection matrix
    virtual glm::mat4 getProjectionMatrix() const = 0;

    // LookAt - uses position from transform as eye, up is world up (0,1,0)
    // center: point to look at (world space)
    virtual void lookAt(const glm::vec3& center) = 0;

    // Projection setup
    virtual void setProjection(float fovDegrees, float aspect, float near, float far) = 0;
};

} // namespace lite
