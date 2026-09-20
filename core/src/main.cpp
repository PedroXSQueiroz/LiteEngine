// NOMINMAX deve vir antes de qualquer include Windows/CEF
#define NOMINMAX

#include <memory.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <functional>
#include <string>
#include <format>
#include <filesystem>

#include <SDL.h>
#include <SDL_syswm.h>
#define SDL_MAIN_HANDLED

#include <assimp/importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>
#include <flatbuffers/data/assets/IO/FlatBuffersSceneSerializer.h>

// New agnostic architecture
#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MaterialData.h>
#include <core/data/assets/Asset3dInstance.h>
#include <core/data/assets/MeshAsset3dInstance.h>
#include <core/data/assets/IO/SceneSerializer.h>
#include <core/data/assets/IO/SceneDTOMapper.h>
#include <core/data/assets/IO/Asset3dDTOMapper.h>
#include <core/data/assets/IO/DummySceneSerializer.h>
#include <core/UI/UIInstance.h>
#include <core/scene/Scene.h>
#include <core/view/View.h>
#include <core/scene/SceneConfigurer.h>
#include <core/scene/SceneFactory.h>
#include <editor/EditorSceneConfigurer.h>
#include <editor/systems/EditorNavigationSystem.h>
#include <assimp/assets/importer/AssimpImporter.h>
#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>
#include <filament/editor/FilamentWireframeSystem.h>
#include <filament/utils/FilamentTransformUtils.h>
#include <filament/utils/FilamentUtils.h>
#include <filament/scene/FilamentScene.h>
#include <filament/scene/FilamentSceneRenderer.h>
#include <filament/scene/FilamentOverlayScene.h>
#include <filament/editor/FilamentEditorSceneConfigurer.h>
#include <filament/view/SDL/SDLFilamentView.h>

#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>
#include <CEF/ui/elements/CEF_UIElements.h>
#include <CEF/ui/CEF_Filament_UIInstance.h>

#include <CEF/CEF_UIApp.h>

// GLM for renderer-agnostic math
#include <glm/glm.hpp>

#include <core/input/InputEvent.h>
#include <filament/editor/FilamentObjectSelectorSystem.h>
#include <filament/editor/FilamentGizmoSystem.h>

#include <core/utils/MathUtils.h>

using namespace std;
using namespace lite;

static INPUT_KEYS sdlKeyToInputKey(SDL_Keycode sym) {
    if (sym >= SDLK_a && sym <= SDLK_z)
        return static_cast<INPUT_KEYS>('A' + (sym - SDLK_a));
    if (sym >= SDLK_0 && sym <= SDLK_9)
        return static_cast<INPUT_KEYS>('0' + (sym - SDLK_0));

    switch (sym) {
        case SDLK_SPACE:      return INPUT_KEYS::KEY_SPACE;
        case SDLK_RETURN:     return INPUT_KEYS::KEY_ENTER;
        case SDLK_TAB:        return INPUT_KEYS::KEY_TAB;
        case SDLK_BACKSPACE:  return INPUT_KEYS::KEY_BACKSPACE;
        case SDLK_ESCAPE:     return INPUT_KEYS::KEY_ESCAPE;
        case SDLK_LSHIFT:     return INPUT_KEYS::KEY_LSHIFT;
        case SDLK_RSHIFT:     return INPUT_KEYS::KEY_RSHIFT;
        case SDLK_LCTRL:      return INPUT_KEYS::KEY_LCTRL;
        case SDLK_RCTRL:      return INPUT_KEYS::KEY_RCTRL;
        case SDLK_LALT:       return INPUT_KEYS::KEY_LALT;
        case SDLK_RALT:       return INPUT_KEYS::KEY_RALT;
        case SDLK_CAPSLOCK:   return INPUT_KEYS::KEY_CAPSLOCK;
        case SDLK_F1:         return INPUT_KEYS::KEY_F1;
        case SDLK_F2:         return INPUT_KEYS::KEY_F2;
        case SDLK_F3:         return INPUT_KEYS::KEY_F3;
        case SDLK_F4:         return INPUT_KEYS::KEY_F4;
        case SDLK_F5:         return INPUT_KEYS::KEY_F5;
        case SDLK_F6:         return INPUT_KEYS::KEY_F6;
        case SDLK_F7:         return INPUT_KEYS::KEY_F7;
        case SDLK_F8:         return INPUT_KEYS::KEY_F8;
        case SDLK_F9:         return INPUT_KEYS::KEY_F9;
        case SDLK_F10:        return INPUT_KEYS::KEY_F10;
        case SDLK_F11:        return INPUT_KEYS::KEY_F11;
        case SDLK_F12:        return INPUT_KEYS::KEY_F12;
        case SDLK_UP:         return INPUT_KEYS::KEY_UP;
        case SDLK_DOWN:       return INPUT_KEYS::KEY_DOWN;
        case SDLK_LEFT:       return INPUT_KEYS::KEY_LEFT;
        case SDLK_RIGHT:      return INPUT_KEYS::KEY_RIGHT;
        case SDLK_INSERT:     return INPUT_KEYS::KEY_INSERT;
        case SDLK_DELETE:     return INPUT_KEYS::KEY_DELETE;
        case SDLK_HOME:       return INPUT_KEYS::KEY_HOME;
        case SDLK_END:        return INPUT_KEYS::KEY_END;
        case SDLK_PAGEUP:     return INPUT_KEYS::KEY_PAGEUP;
        case SDLK_PAGEDOWN:   return INPUT_KEYS::KEY_PAGEDOWN;
        case SDLK_COMMA:      return INPUT_KEYS::KEY_COMMA;
        case SDLK_PERIOD:     return INPUT_KEYS::KEY_PERIOD;
        case SDLK_SLASH:      return INPUT_KEYS::KEY_SLASH;
        case SDLK_SEMICOLON:  return INPUT_KEYS::KEY_SEMICOLON;
        case SDLK_QUOTE:      return INPUT_KEYS::KEY_APOSTROPHE;
        case SDLK_LEFTBRACKET:  return INPUT_KEYS::KEY_LBRACKET;
        case SDLK_RIGHTBRACKET: return INPUT_KEYS::KEY_RBRACKET;
        case SDLK_BACKSLASH:  return INPUT_KEYS::KEY_BACKSLASH;
        case SDLK_MINUS:      return INPUT_KEYS::KEY_MINUS;
        case SDLK_EQUALS:     return INPUT_KEYS::KEY_EQUALS;
        case SDLK_BACKQUOTE:  return INPUT_KEYS::KEY_BACKTICK;
        case SDLK_KP_0:       return INPUT_KEYS::KEY_KP_0;
        case SDLK_KP_1:       return INPUT_KEYS::KEY_KP_1;
        case SDLK_KP_2:       return INPUT_KEYS::KEY_KP_2;
        case SDLK_KP_3:       return INPUT_KEYS::KEY_KP_3;
        case SDLK_KP_4:       return INPUT_KEYS::KEY_KP_4;
        case SDLK_KP_5:       return INPUT_KEYS::KEY_KP_5;
        case SDLK_KP_6:       return INPUT_KEYS::KEY_KP_6;
        case SDLK_KP_7:       return INPUT_KEYS::KEY_KP_7;
        case SDLK_KP_8:       return INPUT_KEYS::KEY_KP_8;
        case SDLK_KP_9:       return INPUT_KEYS::KEY_KP_9;
        case SDLK_KP_ENTER:   return INPUT_KEYS::KEY_KP_ENTER;
        case SDLK_KP_PLUS:    return INPUT_KEYS::KEY_KP_PLUS;
        case SDLK_KP_MINUS:   return INPUT_KEYS::KEY_KP_MINUS;
        case SDLK_KP_MULTIPLY:return INPUT_KEYS::KEY_KP_MULTIPLY;
        case SDLK_KP_DIVIDE:  return INPUT_KEYS::KEY_KP_DIVIDE;
        case SDLK_KP_PERIOD:  return INPUT_KEYS::KEY_KP_PERIOD;
        default:               return INPUT_KEYS::KEY_UNKNOWN;
    }
}



class DummySceneDTOMapper : public SceneDTOMapper<
        FilamentAsset3dInstance,
        FilamentAsset3dTransform,
        FilamentInstanceFactory,
        FilamentScene
>{
    virtual SceneDTO buildBaseSceneDto(
        FilamentScene* scene
    ){
        SceneDTO sceneDto;

        // Nada aqui sai da `scene`: a Scene não tem id, não conhece o IBL (é do
        // SceneRenderer, ver FIXME no SceneDTO) e não guarda os MaterialData
        // depois da instanciação. Enquanto essas fontes não existirem, o dummy
        // repete os mesmos literais que a main usa no setIBL.
        sceneDto.ibl.path      = "D:/Workspace/LiteEngine/3rd_party/filament/out/samples/assets/ibl/lightroom_14b";
        sceneDto.ibl.intensity = 30000.0f;

        // id fica -1 e materials fica vazio: sem fonte de dados hoje.
        return sceneDto;
    }
};

class DummyAsset3dDTOMapper : public Asset3dDTOMapper {

    // Par (entidade, DTO) que este mapper atende: o nó "puro" da cena Filament
    // e o DTO base. typeid do TIPO (não de instância) — é o que o registry
    // compara com typeid(*entity) na hora de escolher o mapper.
    virtual std::type_index getEntityTypeIndex()
    {
        return std::type_index(typeid(FilamentAsset3dInstance));
    }

    virtual std::type_index getDTOTypeIndex()
    {
        return std::type_index(typeid(Asset3dInstanceDTO));
    }

    // Direção de load: sem factory/engine aqui não há como materializar um nó.
    virtual Node* fromDto(const Asset3dInstanceDTO& dto)
    {
        return nullptr;
    }

    virtual std::unique_ptr<Asset3dInstanceDTO> nodeToDto(Node* node)
    {
        auto dto = std::make_unique<Asset3dInstanceDTO>();

        // Os elos da árvore são Node; id/name/visible/transform só existem no
        // Asset3dInstance. Sem o cast não há o que copiar.
        auto* instance = dynamic_cast<Asset3dInstance<FilamentAsset3dTransform>*>(node);
        if (!instance) return dto;

        dto->id             = instance->getId();
        dto->name           = instance->name;
        dto->localTransform = instance->getLocalMatrix();
        dto->visible        = instance->isVisible();

        // children fica vazio de propósito: a recursão é do toDto, não daqui.
        return dto;
    }
};

int main(int argc, char** argv){

    auto importer = std::make_unique<AssimpImporter>();
    
    const std::string SCENE_PATH = "D:/lite_resources/default_scene.le";
    
    /*----------------------------------------------------------------------------
    CEF SUBPROCESS HANDLING - DEVE SER O PRIMEIRO!
    ----------------------------------------------------------------------------*/
    CefMainArgs cef_args(GetModuleHandle(nullptr));
    CefRefPtr<CEF_UIApp> cef_app = new CEF_UIApp();
    int exit_code = CefExecuteProcess(cef_args, cef_app.get(), nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    /*----------------------------------------------------------------------------
    SETUP WINDOW
    ----------------------------------------------------------------------------*/
    const int SCREEN_WIDTH = 1980;
    const int SCREEN_HEIGHT = 1080;

#ifdef _WIN32
    SetEnvironmentVariableA("OPENGL_DRIVER", "opengl32");
#endif

    /*----------------------------------------------------------------------------
    SETUP SCENE RENDERER
    Spawns render thread: engine, swapchain, scene and camera are created there.
    ----------------------------------------------------------------------------*/
    lite::View* view = new SDLFilamentView(SCREEN_WIDTH, SCREEN_HEIGHT);
    view->Init();

    // Tamanho REAL da janela, não os literais: FULLSCREEN_DESKTOP ignora o
    // tamanho pedido e assume a resolução do desktop. O renderer usa estes
    // valores para o viewport E para a aspect da projeção — se divergirem do
    // tamanho usado no pixel→ray do picking, o clique erra o alvo no eixo X.
    glm::vec2 viewDim = view->getDimensions();
    int fbW = viewDim[0], fbH = viewDim[1];

    FilamentSceneRenderer sceneRenderer(view, fbW, fbH);
    sceneRenderer.waitReady();


    auto* currentScene = sceneRenderer.getScene();
    auto* uiRenderer   = currentScene->getCurrentUI();

    std::cout << "Scene renderer ready" << std::endl;

    glm::vec3 center(0, 0, 0);
    float radius = 5.0f;
    glm::vec3 offsetEye    = center + glm::vec3(0, radius * 0.3f, radius);
    glm::vec3 offsetCenter = glm::normalize(center - offsetEye);

    sceneRenderer.setCameraState(offsetEye, offsetEye + offsetCenter);

    int winW = fbW, winH = fbH;
    
    glm::vec2 lastMousePosition = glm::vec2(0, 0);

    
    /*----------------------------------------------------------------------------
    SETUP SERIALIZER
    ----------------------------------------------------------------------------*/
    SceneSerializer* sceneSerialzer = new FlatBuffersSceneSerializer();
    SceneDTOMapper<
        FilamentAsset3dInstance,
        FilamentAsset3dTransform,
        FilamentInstanceFactory,
        FilamentScene>* sceneMapper = new DummySceneDTOMapper();

    sceneMapper->registerMapper( new DummyAsset3dDTOMapper() );
    
    
    /*----------------------------------------------------------------------------
    SETUP UI RENDERER (CEF)
    ----------------------------------------------------------------------------*/
    lite::UIInstance<CEF_Filament_UIRendererThreaded>* uiInstance = new lite::CEF_Filament_UIInstance(uiRenderer);

    sceneRenderer.start();


    std::cout << "Starting main loop..." << std::endl;

    lite::EditorSceneConfigurer<
            FilamentScene, 
            FilamentOverlayScene, 
            FilamentAsset3dTransform,
            FilamentAsset3dInstance,
            FilamentMeshAsset3dInstance,
            FilamentCameraAsset3dInstance,
            FilamentInstanceFactory,
            CEF_Filament_UIRendererThreaded>* 
        configurer = new lite::FilamentEditorSceneConfigurer(
            importer.get(), 
            &sceneRenderer,
            fbW, fbH,
            uiInstance,
            sceneSerialzer,
            sceneMapper
        );

    currentScene = configurer->configure(currentScene);


    /*----------------------------------------------------------------------------
    MAIN LOOP
    ----------------------------------------------------------------------------*/
    bool running = true;

    bool multiple_selection_active = false;

    bool start_object_query_select = false;
    const float VELOCITY_MOVEMENT = radius * 50.f;
    const float VELOCITY_LOOK = 0.003f;
    float horizontal_direction = 0, vertical_direction = 0;

    
    EditorNavigationSystem* navigation                                              = currentScene->getSystemOfType<EditorNavigationSystem>();
    GizmoSystem<FilamentOverlayScene, FilamentAsset3dTransform>* gizmoSystem        = currentScene->getSystemOfType<lite::FilamentGizmoSystem>();
    ObjectSelectorSystem<FilamentScene, FilamentAsset3dTransform>* objectSelector   = currentScene->getSystemOfType<FilamentObjectSelectorSystem>();
    WireframeSystem<FilamentMeshAsset3dInstance>* wireframeSystem                   = currentScene->getSystemOfType<FilamentWireframeSystem>();

    while (running) {
        SDL_Event ev;
        lite::InputEvent inputEvent;
        
        glm::vec3 currentCamLocation = sceneRenderer   
                                .getCurrentCamera()
                                ->getTransform()
                                ->getPosition();
        
        glm::vec3 cameraRayDirection = MathUtils::calcScreenPixelRay(
            sceneRenderer.getCurrentCamera(),
            lastMousePosition,
            glm::vec2(winW, winH),
            10000.0f
        );

        while (SDL_PollEvent(&ev)) {
            navigation->digestInputEvent(ev);
            gizmoSystem->digestInput(ev, lastMousePosition);

            switch(ev.type)
            {
                case SDL_MOUSEMOTION: {
                    lastMousePosition = inputEvent.analogs[lite::INPUT_ANALOGS::MOUSE] =
                        glm::vec2(ev.motion.x, ev.motion.y);

                    break;
                }
                case SDL_MOUSEBUTTONDOWN: {
                    lite::INPUT_KEYS btn = lite::INPUT_KEYS::MOUSE_LEFT;
                    if (ev.button.button == SDL_BUTTON_RIGHT)  btn = lite::INPUT_KEYS::MOUSE_RIGHT;
                    if (ev.button.button == SDL_BUTTON_MIDDLE) btn = lite::INPUT_KEYS::MOUSE_MIDDLE;
                    inputEvent.analogs[lite::INPUT_ANALOGS::MOUSE] = glm::vec2(ev.button.x, ev.button.y);
                    inputEvent.keys[btn] = lite::INPUT_KEY_STATES::DOWN;

                    break;
                }
                case SDL_MOUSEBUTTONUP: {
                    lite::INPUT_KEYS btn = lite::INPUT_KEYS::MOUSE_LEFT;
                    if (ev.button.button == SDL_BUTTON_RIGHT)  btn = lite::INPUT_KEYS::MOUSE_RIGHT;
                    if (ev.button.button == SDL_BUTTON_MIDDLE) btn = lite::INPUT_KEYS::MOUSE_MIDDLE;
                    inputEvent.analogs[lite::INPUT_ANALOGS::MOUSE] = glm::vec2(ev.button.x, ev.button.y);
                    inputEvent.keys[btn] = lite::INPUT_KEY_STATES::UP;
                    
                    break;
                }
                case SDL_KEYDOWN:{
                    INPUT_KEYS key = sdlKeyToInputKey(ev.key.keysym.sym);
                    if (key != INPUT_KEYS::KEY_UNKNOWN)
                        inputEvent.keys[key] = lite::INPUT_KEY_STATES::DOWN;
                    if (ev.key.keysym.sym == SDLK_LSHIFT)  multiple_selection_active = true;
                    break;
                }
                case SDL_KEYUP:{
                    INPUT_KEYS key = sdlKeyToInputKey(ev.key.keysym.sym);
                    if (key != INPUT_KEYS::KEY_UNKNOWN)
                        inputEvent.keys[key] = lite::INPUT_KEY_STATES::UP;
                    if (ev.key.keysym.sym == SDLK_LSHIFT)  multiple_selection_active = false;
                    break;
                }
            }    
        }

        if (uiRenderer) {
            uiRenderer->sendInputEvent(inputEvent);
        }

        //------------------------------------------------------------------------------------------------
        //CLICK TOGGLE
        //------------------------------------------------------------------------------------------------
        if( inputEvent.keys.contains(lite::INPUT_KEYS::MOUSE_LEFT))
        {
            
            //------------------------------------------------------------------------------------------------
            //CLICK PRESSED
            //------------------------------------------------------------------------------------------------
            if(inputEvent.keys[lite::INPUT_KEYS::MOUSE_LEFT] == lite::INPUT_KEY_STATES::DOWN)
            {
                if(!gizmoSystem->getCurrentGizmoAction().has_value())
                {
                    //------------------------------------------------------------------------------------------------
                    //ENCONTRA OBJETO NA CENA
                    //------------------------------------------------------------------------------------------------
                    float coneHalfAngle = sceneRenderer.getCurrentCamera()->getFieldOfViewInRadians() * 0.5f;
                    int objectFoundId = objectSelector->intersect(
                        currentCamLocation,
                        cameraRayDirection,
                        coneHalfAngle
                    );

                    if(objectFoundId != -1)
                    {
                        Asset3dInstance<FilamentAsset3dTransform>* objectFound = sceneRenderer.getScene()->getNode(objectFoundId);

                        if(!multiple_selection_active)
                        {
                            objectSelector->clearSelected();    
                        }

                        objectSelector->addSelected(objectFound);

                        glm::vec3 selectionMedianPoint = objectSelector->getSelectionMedianPoint();

                        gizmoSystem->getGizmoTransform()->setPosition(selectionMedianPoint);
            
                        // clear/addWireframeMesh criam/destroem recursos GPU (CommandStream
                        // do Filament exige a render thread) — postar como comando
                        sceneRenderer.postCommand([wireframe = wireframeSystem, objectFound, multiple_selection_active]() {
                            
                            if(!multiple_selection_active)
                            {
                                wireframe->clearWireframeMeshes();
                            }
                            
                            if (auto* mesh = dynamic_cast<FilamentMeshAsset3dInstance*>(objectFound)) {
                                wireframe->addWireframeMesh(mesh);
                            }
                        });
                    }
                    
        
                    //------------------------------------------------------------------------------------------------
                    //END || ENCONTRA OBJETO NA CENA
                    //------------------------------------------------------------------------------------------------
                }
            }
            //------------------------------------------------------------------------------------------------
            //END || CLICK PRESSED
            //------------------------------------------------------------------------------------------------
        }
        //------------------------------------------------------------------------------------------------
        //END || CLICK TOGGLE
        //------------------------------------------------------------------------------------------------
    }

    std::cout << "Shutting down..." << std::endl;

    /*----------------------------------------------------------------------------
    CLEANUP
    O destrutor do wireframe toca GPU → destruição vai para a render thread via
    postCommand (removendo do scene no mesmo comando, antes do reset). O stop()
    explícito garante que o comando é drenado antes dos locais da main morrerem;
    o stop() do destrutor do sceneRenderer vira no-op (idempotente).
    ----------------------------------------------------------------------------*/
    // if (currentInstanceId >= 0) {
    //     currentScene->destroy(currentInstanceId);   // por id — a Scene é a dona
    // }

    // sceneRenderer.postCommand([&]() {
    //     currentScene->removeSystem(wireframeSystem.get());
    //     wireframeSystem.reset();

    //     // Mesmo motivo: o destrutor do gizmo destrói view/scene/factory do
    //     // overlay (recursos do Engine) — tem de rodar na render thread.
    //     currentScene->removeSystem(gizmoSystem.get());
    //     gizmoSystem.reset();
    // });
    sceneRenderer.stop();

    importer.reset();

    // SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Cleanup complete" << std::endl;

    return 0;
}
