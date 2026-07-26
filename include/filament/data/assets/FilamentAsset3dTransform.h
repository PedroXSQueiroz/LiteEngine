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
    void setPosition(const glm::vec3& position, bool isWorldSpace = false) override;
    glm::vec3 getPosition(bool isWorldSpace = false) override;

    // Rotation
    void setRotation(const glm::quat& rotation, bool isWorldSpace = false) override;
    glm::quat getRotation(bool isWorldSpace = false) override;

    // Scale
    void setScale(const glm::vec3& scale, bool isWorldSpace = false) override;
    glm::vec3 getScale(bool isWorldSpace = false) override;

    // Matrix
    void setLocalMatrix(const glm::mat4& matrix) override;
    //TODO: FAZER VIRTUAL NO PAI?
    void setWorldMatrix(const glm::mat4& matrix);

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
        const glm::vec3* newScale,
        const bool isWorldSpace
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
