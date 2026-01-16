#pragma once

#include <core/data/assets/MeshAsset3dInstance.h>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/MaterialInstance.h>
#include <utils/Entity.h>

namespace lite {

// Filament-specific mesh instance (GPU resources for a single mesh)
class FilamentMeshAsset3dInstance : public MeshAsset3dInstance {
public:
    FilamentMeshAsset3dInstance(filament::Engine* engine, filament::Scene* scene)
        : m_engine(engine)
        , m_scene(scene)
    {}

    ~FilamentMeshAsset3dInstance() override = default;

    void setVisible(bool visible) override;

    // Filament resources for this mesh
    utils::Entity entity;
    filament::VertexBuffer* vertexBuffer = nullptr;
    filament::IndexBuffer* indexBuffer = nullptr;
    filament::MaterialInstance* materialInstance = nullptr;

    // Access to engine/scene for operations
    filament::Engine* getEngine() const { return m_engine; }
    filament::Scene* getScene() const { return m_scene; }

private:
    filament::Engine* m_engine;
    filament::Scene* m_scene;
};

} // namespace lite
