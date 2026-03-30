#pragma once

#include <core/data/assets/Asset3dInstance.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>

#include <vector>
#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/MaterialInstance.h>
#include <filament/Texture.h>
#include <utils/Entity.h>

#include <glm/glm.hpp>

namespace lite {

// Filament-specific root instance (container for mesh instances)
// Uses hierarchy from Asset3dInstance - meshes are FilamentMeshAsset3dInstance children
class FilamentAsset3dInstance : public Asset3dInstance<FilamentAsset3dTransform> {
public:
    FilamentAsset3dInstance(
            utils::Entity entity
        ,   FilamentAsset3dTransform transform)
        :   Asset3dInstance(std::make_unique<FilamentAsset3dTransform>(transform))
        ,   m_entity(entity){

    };

    ~FilamentAsset3dInstance() override = default;

    void setVisible(bool visible) override;

    // Entity for this node (used for transform hierarchy)
    utils::Entity getEntity() const { return m_entity; }

    //TODO IA: Isso deveria estar em um serviço de Materiais
    // Shared material instances (ownership - destroyed by factory)
    std::vector<filament::MaterialInstance*> materialInstances;

    //TODO IA: Isso deveria estar em um serviço de Assets
    // Shared textures (referenced from cache, not owned)
    std::vector<filament::Texture*> textures;

    void markAsDeleted() { this->m_isDeleted = true; }

    bool isDeleted() { return this->m_isDeleted; }

private:
    
    utils::Entity m_entity;

    bool m_isDeleted { false };
};

} // namespace lite
