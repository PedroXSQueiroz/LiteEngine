#pragma once

#include <core/scene/SceneRenderer.h>
#include <core/scene/SceneConfigurer.h>
#include <core/SceneScopeSystem.h>
#include <core/ui/elements/UIElements.h>
#include <core/assets/importer/Asset3dImporter.h>

#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MaterialData.h>

#include <core/utils/TransformUtils.h>

#include <editor/GizmoSystem.h>
#include <editor/WireframeSystem.h>
#include <editor/ObjectSelectorSystem.h>
#include <editor/systems/EditorNavigationSystem.h>

#include <memory>
#include <vector>

namespace lite{

    template<
        SceneConcept SceneType, 
        SceneConcept SceneOverlayGizmoType, 
        TransformConcept TransformType,
        Asset3dConcept AssetType,
        MeshAsset3dConcept MeshType,
        CameraConcept CameraType,
        typename UIRendererType //TODO: CRIAR UM CONCEPT PARA OBRIGAR ESSE TEMPLATE A HERADR DE UIRENDERER?
    >
    class EditorSceneConfigurer : public SceneConfigurer<SceneType>
    {
        public:

        EditorSceneConfigurer(
                Asset3dImporter* importer
            ,   SceneRenderer<SceneType, CameraType>* renderer
        ): m_assets3dImporter(importer), m_renderer(renderer) {};

        virtual SceneType* configure(SceneType* scene) override {
            
            scene->addSystem(getGizmoSystem(scene));
            scene->addSystem(getWireframeSystem(scene));
            scene->addSystem(getObjectSelectorSystem());
            scene->addSystem(getNavigationSystem());

            scene = configureUI(scene);

            scene = loadScene3dInstances(scene);

            /*----------------------------------------------------------------------------
            SETUP LIGHTING (posted to render thread command queue)
            ----------------------------------------------------------------------------*/
            std::string iblPath = "D:/Workspace/LiteEngine/3rd_party/filament/out/samples/assets/ibl/lightroom_14b";
            m_renderer->setIBL(iblPath, 30000.0f);

            m_renderer->addDirectionalLight(
                {1.0f, 1.0f, 0.95f},
                100000.0f,
                {0.6f, -1.0f, -0.8f},
                false
            );
            
            return scene;    
        };

        public:
        //--------------------------------------------------------------------------------------------------------
        //SYSTEMS
        //--------------------------------------------------------------------------------------------------------
        virtual std::unique_ptr<GizmoSystem<SceneOverlayGizmoType, TransformType>> getGizmoSystem(SceneType* scene) = 0;

        virtual std::unique_ptr<WireframeSystem<MeshType>> getWireframeSystem(SceneType* scene) = 0;

        virtual std::unique_ptr<ObjectSelectorSystem<SceneType, TransformType>> getObjectSelectorSystem() = 0;

        virtual std::unique_ptr<EditorNavigationSystem> getNavigationSystem() = 0;
        
        SceneRenderer<SceneType, CameraType>* m_renderer;
        //--------------------------------------------------------------------------------------------------------
        //UI
        //--------------------------------------------------------------------------------------------------------
        protected:
        virtual UIRendererType* getUIRenderer() = 0;
        
        virtual UIInstance<UIRendererType>* getUIInstance(SceneType* scene) = 0;

        virtual UIPanelElement<UIRendererType>* createPanel(UIRendererType* renderer) = 0;
        
        virtual UITextElement<UIRendererType>* createText(UIRendererType* renderer) = 0;

        virtual UITextInputElement<UIRendererType>* createTextInput(UIRendererType* renderer, std::string label) = 0;

        virtual UIButtonElement<UIRendererType>* createButton(UIRendererType* renderer, std::string label) = 0;

        SceneType* configureUI(SceneType* scene){
            UIRendererType* uiRenderer = getUIRenderer();
            UIInstance<UIRendererType>* uiInstance = this->getUIInstance(scene);

            UIPanelElement<UIRendererType>* root = nullptr;
            if (!(root = uiInstance->start())) {
                std::cerr << "Failed to start UIRenderer" << std::endl;
                return nullptr;
            }

            UIPanelElement<UIRendererType>* leftPanel = createPanel(uiRenderer);
            root->addChildComponent(leftPanel, 0, 0);

            UITextElement<UIRendererType>* uiText = createText(uiRenderer);
            leftPanel->addChildComponent(uiText, 0, 0, 1, 2);

            UITextInputElement<UIRendererType>* uiInput = createTextInput(uiRenderer, "Modelo");
            leftPanel->addChildComponent(uiInput, 1, 0, 1, 2);


            UIButtonElement<UIRendererType>* loadModelButton = createButton(uiRenderer, "Carregar");
            loadModelButton->registerEvent("click", [&](UIRendererType*, int, std::string) {
                std::cout << "load model invoked" << std::endl;
                // if (importer->import(uiInput->getText(), rootNode, materials)) {
                //     currentInstanceId = currentScene->create(
                //         rootNode,
                //         materials,
                //         TransformUtils<FilamentAsset3dTransform>::build()
                //     );
                //     assetPtr = currentScene->get(currentInstanceId);
                // }
            });

            UIButtonElement<UIRendererType>* deleteModelButton = createButton(uiRenderer, "Deletar");
            deleteModelButton->registerEvent("click", [&](UIRendererType*, int, std::string) {
                // if (currentScene->destroy(currentInstanceId)) {
                //     std::cout << "current model deleted" << std::endl;
                // } else {
                //     std::cout << "something went wrong on deletion" << std::endl;
                // }
            });

            UIButtonElement<UIRendererType>* saveSceneButton = createButton(uiRenderer, "Salvar");
            saveSceneButton->registerEvent("click",  [&](UIRendererType*, int, std::string){

                // SceneDTO sceneDto = sceneMapper->toDto(currentScene);
                // sceneSerialzer->save(sceneDto, SCENE_PATH);

            });

            leftPanel->addChildComponent(loadModelButton, 2, 0);
            leftPanel->addChildComponent(deleteModelButton, 2, 1);
            leftPanel->addChildComponent(saveSceneButton, 3, 0);

            root->draw();

            return scene;
        };

        //--------------------------------------------------------------------------------------------------------
        //OBJECTS
        //--------------------------------------------------------------------------------------------------------
        Asset3dImporter* m_assets3dImporter;
        
        SceneType* loadScene3dInstances(SceneType* scene){
            Asset3dData rootNode;
            std::vector<std::unique_ptr<MaterialData>> materials;
            int currentInstanceId = -1;
            AssetType* assetPtr = nullptr;

            if (m_assets3dImporter->import(
                // "D:/Workspace/LiteEngine/test-resources/simple_sphere.fbx"
                "C:/Users/pixqu/Downloads/Jason Stalhart/Base_Mesh/Aiden_Stallhart_BaseMesh_skeleton_Ver1.fbx"
                , rootNode, materials)) {
                currentInstanceId = scene->create(
                    rootNode,
                    materials,
                    TransformUtils<TransformType>::build(),
                    true
                );
                assetPtr = scene->get(currentInstanceId);
            }

            return scene;
        };

    };

}