#pragma once

#include <editor/EditorSceneConfigurer.h>
#include <filament/scene/FilamentScene.h>
#include <filament/scene/FilamentSceneRenderer.h>
#include <filament/scene/FilamentOverlayScene.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>
#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/data/assets/FilamentMeshAsset3dInstance.h>
#include <filament/editor/FilamentWireframeSystem.h>
#include <filament/editor/FilamentGizmoSystem.h>
#include <filament/editor/FilamentObjectSelectorSystem.h>
#include <filament/utils/FilamentUtils.h>
#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>
#include <CEF/ui/CEF_Filament_UIInstance.h>
#include <CEF/ui/elements/CEF_UIElements.h>

#include <editor/GizmoSystem.h>
#include <editor/WireframeSystem.h>
#include <editor/ObjectSelectorSystem.h>
#include <editor/systems/EditorNavigationSystem.h>

namespace lite{

    class FilamentEditorSceneConfigurer: public EditorSceneConfigurer<
        FilamentScene,
        FilamentOverlayScene,
        FilamentAsset3dTransform,
        FilamentAsset3dInstance,
        FilamentMeshAsset3dInstance,
        FilamentCameraAsset3dInstance,
        FilamentInstanceFactory,
        CEF_Filament_UIRendererThreaded
    >
    {
    public:
        
        FilamentEditorSceneConfigurer(
                Asset3dImporter* importer
            ,   FilamentSceneRenderer* renderer
            ,   int width
            ,   int height
            ,   UIInstance<CEF_Filament_UIRendererThreaded>* uiInstance
            ,   SceneSerializer* serializer
            ,   SceneDTOMapper<
                    FilamentAsset3dInstance
                ,   FilamentAsset3dTransform
                ,   FilamentInstanceFactory
                ,   FilamentScene
                >* sceneMapper
            ):
        EditorSceneConfigurer(importer, renderer, serializer, sceneMapper),
        m_width(width),
        m_height(height),
        m_uiInstance(uiInstance)
        {};

    private:
        //--------------------------------------------------------------------------------------------------------
        //SYSTEMS
        //--------------------------------------------------------------------------------------------------------
        virtual std::unique_ptr<GizmoSystem<FilamentOverlayScene, FilamentAsset3dTransform>> getGizmoSystem(FilamentScene* scene) override {
             GizmoParts gizmo = lite::gizmo::buildGizmoParts(
                m_assets3dImporter,
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_move_x.fbx",
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_move_y.fbx",
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_move_z.fbx",
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_rotate_x.fbx",
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_rotate_y.fbx",
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_rotate_z.fbx",
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_scale_x.fbx",
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_scale_y.fbx",
                "C:/Users/pixqu/Downloads/transform_gizmo (1)/gizmo_scale_z.fbx"
            );

            // TODO: CALCULAR ISSO FORA DO CLIQUE PODE SER "CARO", REVER ISSO
            // // Posição do clique capturada no evento (ev já pode ser outro evento
            // // do mesmo frame); viewport real da janela (FULLSCREEN_DESKTOP =
            // // resolução do desktop, não SCREEN_WIDTH/HEIGHT)
            // int winW = fbW, winH = fbH;
            // SDL_GetWindowSize(window, &winW, &winH);
            std::unique_ptr<lite::FilamentGizmoSystem> gizmoSystem =
                std::make_unique<lite::FilamentGizmoSystem>(
                    FilamentUtils::getEngine(),
                    std::move(gizmo),
                    m_height, m_width
                );
            gizmoSystem->setCamera(
                m_renderer->getCurrentCamera());
            gizmoSystem->attachTo(scene);
            
            return gizmoSystem;
        };
    
        virtual std::unique_ptr<WireframeSystem<FilamentMeshAsset3dInstance>> getWireframeSystem(FilamentScene* scene) override {
            std::unique_ptr<FilamentWireframeSystem> wireframeSystem =
                std::make_unique<FilamentWireframeSystem>(
                    FilamentUtils::getEngine(),
                    scene->getFilamentScene()
                );

            wireframeSystem->initialize(m_width, m_height);
            wireframeSystem->setMaterialPath("D:/Workspace/LiteEngine/core/resources/filament/editor/materials/wireframe.filamat");
            wireframeSystem->setWireframeColor(glm::vec4(1.0f, 1.0f, 1.0f, 0.6f));
            wireframeSystem->attachTo(scene);
            
            return wireframeSystem;
        };

        virtual std::unique_ptr<ObjectSelectorSystem<FilamentScene, FilamentAsset3dTransform>> getObjectSelectorSystem() override {
            std::unique_ptr<FilamentObjectSelectorSystem> objectSelector = std::make_unique<FilamentObjectSelectorSystem>();
            objectSelector->setCamera(m_renderer->getCurrentCamera());
            objectSelector->attachTo(m_renderer->getScene());

            return objectSelector;
        };

        virtual std::unique_ptr<EditorNavigationSystem> getNavigationSystem() override {
            std::unique_ptr<EditorNavigationSystem> navigation = std::make_unique<EditorNavigationSystem>(
                [&](glm::vec3 center, glm::vec3 offsetCenter, glm::vec3 offsetEye){
                    m_renderer->setCameraState(offsetEye, offsetEye + offsetCenter);

                    return  m_renderer   
                            ->getCurrentCamera()
                            ->getTransform()
                            ->getPosition();
            });
            
            return navigation;
        };

        //--------------------------------------------------------------------------------------------------------
        //UI
        //--------------------------------------------------------------------------------------------------------
        // O renderer de UI é criado pelo FilamentSceneRenderer::setup() na render
        // thread e pertence à Scene; aqui só se devolve o que já existe.
        virtual CEF_Filament_UIRendererThreaded* getUIRenderer() override {
            return m_renderer->getScene()->getCurrentUI();
        };

        virtual UIInstance<CEF_Filament_UIRendererThreaded>* getUIInstance(FilamentScene* scene) override {
            return m_uiInstance;
        };

        virtual UIPanelElement<CEF_Filament_UIRendererThreaded>* createPanel(CEF_Filament_UIRendererThreaded* renderer) override {
            return new CEF_UIPanelElement(renderer);
        };

        virtual UITextElement<CEF_Filament_UIRendererThreaded>* createText(CEF_Filament_UIRendererThreaded* renderer) override {
            return new CEF_UITextElement(renderer);
        };

        virtual UITextInputElement<CEF_Filament_UIRendererThreaded>* createTextInput(CEF_Filament_UIRendererThreaded* renderer, std::string label) override {
            return new CEF_UITextInputElement(renderer, label);
        };

        virtual UIButtonElement<CEF_Filament_UIRendererThreaded>* createButton(CEF_Filament_UIRendererThreaded* renderer, std::string label) override {
            return new CEF_UIButtonElement(renderer, label);
        };

        virtual UITreeElement<CEF_Filament_UIRendererThreaded, Node*>* createTreeView(CEF_Filament_UIRendererThreaded* renderer) override {
            return new CEF_UITreeElement<Node*>(renderer);
        }

        int m_width = 0, m_height = 0;

        UIInstance<CEF_Filament_UIRendererThreaded>* m_uiInstance = nullptr;

    };
}
