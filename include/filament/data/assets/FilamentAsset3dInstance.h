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
class FilamentAsset3dInstance : public Asset3dInstance {
public:
    FilamentAsset3dInstance(filament::Engine* engine, filament::Scene* scene);

    ~FilamentAsset3dInstance() override = default;

    void setVisible(bool visible) override;

    // Initialize transform after entity is created
    void initializeTransform(utils::Entity entity);

    // Access to engine/scene for operations
    filament::Engine* getEngine() const { return m_engine; }
    filament::Scene* getScene() const { return m_scene; }

    // Entity for this node (used for transform hierarchy)
    utils::Entity getEntity() const { return m_entity; }

    // Shared material instances (ownership - destroyed by factory)
    std::vector<filament::MaterialInstance*> materialInstances;

    // Shared textures (referenced from cache, not owned)
    std::vector<filament::Texture*> textures;

private:
    filament::Engine* m_engine;
    filament::Scene* m_scene;
    utils::Entity m_entity;
};

} // namespace lite
