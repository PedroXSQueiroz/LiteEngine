#pragma once

#include <core/data/assets/Asset3dTransform.h>
#include <filament/TransformManager.h>
#include <utils/Entity.h>

namespace lite {

// Filament implementation - pure facade to TransformManager
class FilamentAsset3dTransform : public Asset3dTransform {
public:
    FilamentAsset3dTransform(filament::TransformManager& transformManager, utils::Entity entity);
    ~FilamentAsset3dTransform() override = default;

    // Position
    void setPosition(const glm::vec3& position) override;
    glm::vec3 getPosition() const override;

    // Rotation
    void setRotation(const glm::quat& rotation) override;
    glm::quat getRotation() const override;

    // Scale
    void setScale(const glm::vec3& scale) override;
    glm::vec3 getScale() const override;

    // Matrix
    void setLocalMatrix(const glm::mat4& matrix) override;
    glm::mat4 getLocalMatrix() const override;
    glm::mat4 getWorldMatrix() const override;

    utils::Entity getEntity() const { return m_entity; }

private:
    void modifyComponent(
        const glm::vec3* newPosition,
        const glm::quat* newRotation,
        const glm::vec3* newScale
    );

    filament::TransformManager& m_transformManager;
    utils::Entity m_entity;
};

} // namespace lite
