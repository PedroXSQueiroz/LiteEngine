#pragma once

#include <editor/WireframeSystem.h>
#include <filament/data/assets/FilamentMeshAsset3dInstance.h>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <utils/Entity.h>

#include <vector>
#include <string>

namespace lite {

// Filament-specific implementation of WireframeSystem
// Uses barycentric coordinates in vertex color to draw wireframe overlay
class FilamentWireframeSystem : public WireframeSystem {
public:
    explicit FilamentWireframeSystem(filament::Engine* engine, filament::Scene* scene);
    ~FilamentWireframeSystem() override;

    // WireframeSystem interface implementation
    void initialize(uint32_t width, uint32_t height) override;
    void resize(uint32_t width, uint32_t height) override;
    void setWireframeColor(const glm::vec4& color) override;
    void setWireframeWidth(float width) override;
    void addWireframeMesh(MeshAsset3dInstance* mesh) override;
    void removeWireframeMesh(MeshAsset3dInstance* mesh) override;
    void clearWireframeMeshes() override;
    void beginFrame() override;

    // Filament-specific methods
    void loadMaterial(const std::string& materialPath);

private:
    void updateWireframeEntities();

    // Wireframe entity data - duplicates mesh with barycentric coords
    struct WireframeEntity {
        utils::Entity entity;
        FilamentMeshAsset3dInstance* sourceMesh;
        filament::VertexBuffer* vertexBuffer;
        filament::IndexBuffer* indexBuffer;
    };

    filament::Engine* m_engine;
    filament::Scene* m_scene;

    // Material
    filament::Material* m_wireframeMaterial = nullptr;
    filament::MaterialInstance* m_wireframeMaterialInstance = nullptr;

    // Wireframe entities (duplicates of source meshes with barycentric coords)
    std::vector<WireframeEntity> m_wireframeEntities;

    bool m_initialized = false;
};

} // namespace lite
