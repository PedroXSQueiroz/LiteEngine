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
#include <functional>

class FilamentScene;

namespace lite {

// Filament-specific implementation of WireframeSystem
// Uses barycentric coordinates in vertex color to draw wireframe overlay
//
// Uso (configuração 100% main thread — recursos GPU são criados de forma
// preguiçosa no primeiro hook, que roda na render thread):
//   wireframe = make_unique<FilamentWireframeSystem>(engine, fScene);
//   wireframe->initialize(w, h);
//   wireframe->setMaterialPath(".../wireframe.filamat");
//   wireframe->setWireframeColor({...});
//   wireframe->attachTo(liteScene);
//   liteScene->addSystem(wireframe.get());   // antes de SceneRenderer::start()
//
// THREADING: o destrutor destrói recursos GPU — destruir via postCommand
// (render thread), removendo antes do Scene::removeSystem no mesmo comando.
class FilamentWireframeSystem : public WireframeSystem<FilamentMeshAsset3dInstance>{
public:
    explicit FilamentWireframeSystem(filament::Engine* engine, filament::Scene* scene);
    ~FilamentWireframeSystem() override;

    // Liga o sistema à cena que ele observa (auto-track e limpeza de deletados)
    void attachTo(FilamentScene* liteScene) { m_liteScene = liteScene; }

    // Rastreio automático de meshes de assets vivos da cena. Default: ligado.
    void setAutoTrack(bool enabled) { m_autoTrack = enabled; }

    // Guarda o path do material; o load GPU acontece no primeiro hook
    // (render thread). Substitui chamar loadMaterial direto da main.
    void setMaterialPath(const std::string& materialPath) { m_materialPath = materialPath; }

    // --- Hooks do lifecycle (executam na render thread) ---
    void onFrameBegin(float deltaTime) override;   // lazy GPU init + auto-track
    // preRenderScene → update() herdado de WireframeSystem (sync de transforms)
    void onFrameEnd(float deltaTime) override;     // remove wireframes de assets deletados

    // WireframeSystem interface implementation
    void initialize(uint32_t width, uint32_t height) override;
    void resize(uint32_t width, uint32_t height) override;
    void setWireframeColor(const glm::vec4& color) override;
    void setWireframeWidth(float width) override;
    void addWireframeMesh(FilamentMeshAsset3dInstance* mesh) override;
    void removeWireframeMesh(FilamentMeshAsset3dInstance* mesh) override;
    void clearWireframeMeshes() override;
    void update() override;

    // Filament-specific methods
    // THREADING: cria recursos GPU — só chamar na render thread (os hooks já
    // fazem isso via setMaterialPath + lazy init)
    void loadMaterial(const std::string& materialPath);

private:
    void updateWireframeEntities();

    // Materialização preguiçosa do material (render thread)
    void ensureGpuResources();

    // Percorre a árvore de um asset aplicando fn a cada mesh
    void forEachMesh(
        Asset3dInstance<FilamentAsset3dTransform>& node,
        const std::function<void(FilamentMeshAsset3dInstance*)>& fn
    );

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

    // Integração com a cena (lifecycle)
    FilamentScene* m_liteScene = nullptr;
    bool m_autoTrack = true;
    std::string m_materialPath;
    bool m_gpuReady = false;
};

} // namespace lite
