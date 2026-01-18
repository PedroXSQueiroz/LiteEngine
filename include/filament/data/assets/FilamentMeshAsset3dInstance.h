#pragma once

#include <core/data/assets/MeshAsset3dInstance.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/MaterialInstance.h>
#include <utils/Entity.h>

#include <glm/glm.hpp>
#include <vector>

namespace lite {

// Filament-specific mesh instance (GPU resources for a single mesh)
class FilamentMeshAsset3dInstance : public MeshAsset3dInstance {
public:
    FilamentMeshAsset3dInstance(filament::Engine* engine, filament::Scene* scene);

    ~FilamentMeshAsset3dInstance() override = default;

    void setVisible(bool visible) override;

    // Initialize transform after entity is created
    void initializeTransform(utils::Entity entity);

    // Filament resources for this mesh
    utils::Entity getEntity() const { return m_entity; }
    filament::VertexBuffer* vertexBuffer = nullptr;
    filament::IndexBuffer* indexBuffer = nullptr;
    filament::MaterialInstance* materialInstance = nullptr;

    // CPU-side geometry data (kept for wireframe/debug rendering)
    std::vector<glm::vec3> cpuPositions;
    std::vector<glm::vec3> cpuNormals;
    std::vector<uint32_t> cpuIndices;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);

    // Access to engine/scene for operations
    filament::Engine* getEngine() const { return m_engine; }
    filament::Scene* getScene() const { return m_scene; }

private:
    filament::Engine* m_engine;
    filament::Scene* m_scene;
    utils::Entity m_entity;
};

} // namespace lite
