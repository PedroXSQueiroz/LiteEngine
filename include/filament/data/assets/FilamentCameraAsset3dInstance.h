#pragma once

#include <core/data/assets/CameraAsset3dInstance.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>

#include <filament/Engine.h>
#include <filament/Camera.h>
#include <utils/Entity.h>

namespace lite {

class FilamentCameraAsset3dInstance : public CameraAsset3dInstance<FilamentAsset3dTransform> {
public:
    FilamentCameraAsset3dInstance(filament::Engine* engine);
    ~FilamentCameraAsset3dInstance() override;

    // CameraAsset3dInstance interface
    glm::mat4 getViewMatrix() const override;
    glm::mat4 getProjectionMatrix() const override;
    void lookAt(const glm::vec3& center) override;
    void setProjection(float fovDegrees, float aspect, float near, float far) override;

    // Filament-specific access (for View setup)
    filament::Camera* getFilamentCamera() const { return m_camera; }
    utils::Entity getEntity() const { return m_entity; }

    // Initialize transform (called after construction)
    void initializeTransform();

private:
    filament::Engine* m_engine;
    filament::Camera* m_camera = nullptr;
    utils::Entity m_entity;
};

} // namespace lite
