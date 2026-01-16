#pragma once

#include <core/data/assets/Asset3dInstance.h>

#include <vector>
#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/MaterialInstance.h>
#include <filament/Texture.h>

#include <glm/glm.hpp>

namespace lite {

// Filament-specific root instance (container for mesh instances)
// Uses hierarchy from Asset3dInstance - meshes are FilamentMeshAsset3dInstance children
class FilamentAsset3dInstance : public Asset3dInstance {
public:
    FilamentAsset3dInstance(filament::Engine* engine, filament::Scene* scene)
        : m_engine(engine)
        , m_scene(scene)
    {}

    ~FilamentAsset3dInstance() override = default;

    void setVisible(bool visible) override;

    // Set transform on root (affects all children)
    void setTransform(const glm::mat4& transform);

    // Access to engine/scene for operations
    filament::Engine* getEngine() const { return m_engine; }
    filament::Scene* getScene() const { return m_scene; }

    // Shared material instances (ownership - destroyed by factory)
    std::vector<filament::MaterialInstance*> materialInstances;

    // Shared textures (referenced from cache, not owned)
    std::vector<filament::Texture*> textures;

private:
    filament::Engine* m_engine;
    filament::Scene* m_scene;
    glm::mat4 m_transform = glm::mat4(1.0f);
};

} // namespace lite
