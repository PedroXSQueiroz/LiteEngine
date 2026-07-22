#pragma once

#include <editor/GizmoSystem.h>
#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/data/assets/FilamentCameraAsset3dInstance.h>
#include <filament/scene/FilamentOverlayScene.h>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <utils/Entity.h>

#include <memory>

namespace lite {

// Implementação Filament do GizmoSystem: possui a cena de overlay do gizmo
// (filament::Scene + View próprias) e a desenha num render pass extra, dentro
// do frame já aberto pela cena principal.
//
// Uso (configuração 100% main thread — nada de GPU acontece aqui; a criação é
// preguiçosa no primeiro hook, que roda na render thread):
//   GizmoParts parts;  // 9 modelos importados pela main
//   gizmo = make_unique<FilamentGizmoSystem>(engine, std::move(parts));
//   gizmo->attachTo(liteScene);
//   gizmo->setCamera(sceneRenderer.getCurrentCamera());
//   liteScene->addSystem(gizmo.get());   // antes de SceneRenderer::start()
//
// THREADING: o destrutor destrói recursos GPU (view, scene, factory da cena de
// overlay) — destruir via postCommand (render thread), removendo antes o system
// da cena no mesmo comando, como já é feito com o FilamentWireframeSystem.
class FilamentGizmoSystem
    : public GizmoSystem<FilamentOverlayScene, FilamentAsset3dTransform> {
public:
    FilamentGizmoSystem(filament::Engine* engine, GizmoParts parts);
    ~FilamentGizmoSystem() override;

    // Cena principal: de onde vêm o filament::Renderer (o pass do overlay entra
    // no frame dela) e o viewport de referência.
    void attachTo(::FilamentScene* liteScene) { m_liteScene = liteScene; }

    // Câmera da cena 3D — a view do overlay usa a MESMA câmera, então não há
    // nada a sincronizar por frame.
    void setCamera(FilamentCameraAsset3dInstance* camera) { m_camera = camera; }

    // Cena de overlay (dona das peças). Null até o primeiro frame.
    FilamentOverlayScene* getOverlayScene() { return m_gizmoScene.get(); }

    filament::View* getView() { return m_view; }

protected:
    // --- Contrato do GizmoSystem: tudo executa na render thread ---
    bool initializeOverlay() override;
    std::unique_ptr<NodeType> createRoot() override;
    int  createPart(const Asset3dData& data,
                    const std::vector<MaterialData>& materials) override;
    void updateOverlay(float deltaTime) override;
    void renderOverlay() override;
    void attachPartToRoot(int partId) override;

private:
    filament::Engine* m_engine;

    // Dependências injetadas pela main antes do start
    ::FilamentScene* m_liteScene = nullptr;
    FilamentCameraAsset3dInstance* m_camera = nullptr;

    // Recursos do overlay (criados no primeiro hook, na render thread)
    filament::Scene* m_filamentScene = nullptr;
    filament::View* m_view = nullptr;
    std::unique_ptr<FilamentOverlayScene> m_gizmoScene;

    // Entity do root (o nó em si é possuído pela base, via createRoot).
    // Guardada aqui porque o parentesco e a destruição são trabalho do renderer.
    utils::Entity m_rootEntity;
};

} // namespace lite
