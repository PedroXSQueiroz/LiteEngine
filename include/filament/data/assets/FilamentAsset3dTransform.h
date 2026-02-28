#pragma once

#include <core/data/assets/Asset3dTransform.h>
#include <filament/TransformManager.h>
#include <utils/Entity.h>

#include <optional>

namespace lite {

// Filament implementation - pure facade to TransformManager
class FilamentAsset3dTransform : public Asset3dTransform {
public:
    FilamentAsset3dTransform(
            filament::TransformManager& transformManager
        ,   std::optional<utils::Entity> entity = std::nullopt
    );
    ~FilamentAsset3dTransform() override = default;

    // Position
    void setPosition(const glm::vec3& position) override;
    glm::vec3 getPosition() override;

    // Rotation
    void setRotation(const glm::quat& rotation) override;
    glm::quat getRotation() override;

    // Scale
    void setScale(const glm::vec3& scale) override;
    glm::vec3 getScale() override;

    // Matrix
    void setLocalMatrix(const glm::mat4& matrix) override;
    glm::mat4 getLocalMatrix() override;
    glm::mat4 getWorldMatrix() override;

    utils::Entity getEntity() { 
        this->assertEntity();
        return m_entity.value();
    };

    void of(utils::Entity entity)
    {
        this->m_entity.emplace(entity);
    };


private:
    void modifyComponent(
        const glm::vec3* newPosition,
        const glm::quat* newRotation,
        const glm::vec3* newScale
    );

    void assertEntity()
    {
        if(!this->m_entity.has_value())
        {
            throw "[filament] Entity not setted for transform";
        }
    };

    filament::TransformManager& m_transformManager;
    std::optional<utils::Entity> m_entity;
};

} // namespace lite
