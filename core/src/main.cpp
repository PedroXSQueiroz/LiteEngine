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
#include <editor/systems/EditorNavigationSystem.h>
#include <assimp/assets/importer/AssimpImporter.h>
#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>
#include <filament/editor/FilamentWireframeSystem.h>
#include <filament/utils/FilamentTransformUtils.h>
#include <filament/utils/FilamentUtils.h>
#include <filament/scene/FilamentScene.h>
#include <filament/scene/FilamentSceneRenderer.h>

#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>
#include <CEF/ui/elements/CEF_UIElements.h>
#include <CEF/ui/CEF_Filament_UIInstance.h>

#include <CEF/CEF_UIApp.h>

// GLM for renderer-agnostic math
#include <glm/glm.hpp>

#include <core/input/InputEvent.h>
#include <filament/editor/FilamentObjectSelectorSystem.h>
#include <filament/editor/FilamentGizmoSystem.h>

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

static void* getNativeWindowHandle(SDL_Window* window) {
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi)) {
        return nullptr;
    }
#if defined(_WIN32)
    return (void*) wmi.info.win.window;
#elif defined(__APPLE__)
    return wmi.info.cocoa.window;
#else
    return (void*) (uintptr_t) wmi.info.x11.window;
#endif
}

class DummySceneDTOMapper : public SceneDTOMapper<
        FilamentAsset3dInstance,
        FilamentAsset3dTransform,
        FilamentInstanceFactory,
        CEF_Filament_UIRendererThreaded
>{
    virtual SceneDTO buildBaseSceneDto(
        lite::Scene<
            FilamentAsset3dInstance,
            FilamentAsset3dTransform,
            FilamentInstanceFactory,
            CEF_Filament_UIRendererThreaded>* scene
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

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

#ifdef _WIN32
    SetEnvironmentVariableA("OPENGL_DRIVER", "opengl32");
#endif

    SDL_Window* window = SDL_CreateWindow(
        "Lite",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_ALLOW_HIGHDPI |
        SDL_WINDOW_SHOWN |
        SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_FULLSCREEN_DESKTOP
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    std::cout << "Window created successfully" << std::endl;

    void* nativeWindow = getNativeWindowHandle(window);
    if (!nativeWindow) {
        std::cerr << "Failed to get native window handle" << std::endl;
        return -1;
    }

    int fbW = SCREEN_WIDTH, fbH = SCREEN_HEIGHT;
    SDL_GetWindowSize(window, &fbW, &fbH);

    /*----------------------------------------------------------------------------
    SETUP SCENE RENDERER
    Spawns render thread: engine, swapchain, scene and camera are created there.
    ----------------------------------------------------------------------------*/
    FilamentSceneRenderer sceneRenderer(nativeWindow, fbW, fbH);
    sceneRenderer.waitReady();

    auto* currentScene = sceneRenderer.getScene();
    auto* uiRenderer   = currentScene->getCurrentUI();

    std::cout << "Scene renderer ready" << std::endl;

    /*----------------------------------------------------------------------------
    SETUP LIGHTING (posted to render thread command queue)
    ----------------------------------------------------------------------------*/
    std::string iblPath = "D:/Workspace/LiteEngine/3rd_party/filament/out/samples/assets/ibl/lightroom_14b";
    sceneRenderer.setIBL(iblPath, 30000.0f);

    sceneRenderer.addDirectionalLight(
        {1.0f, 1.0f, 0.95f},
        100000.0f,
        {0.6f, -1.0f, -0.8f},
        false
    );

    /*----------------------------------------------------------------------------
    SETUP WIREFRAME SYSTEM
    Configuração é 100% CPU (main thread); os recursos GPU são criados de forma
    preguiçosa pelo próprio sistema no primeiro hook, já na render thread.
    Registro via addSystem ANTES de sceneRenderer.start() — regra dos systems.
    ----------------------------------------------------------------------------*/
    std::unique_ptr<FilamentWireframeSystem> wireframeSystem =
        std::make_unique<FilamentWireframeSystem>(
            FilamentUtils::getEngine(),
            currentScene->getFilamentScene()
        );
    wireframeSystem->initialize(fbW, fbH);
    wireframeSystem->setMaterialPath("D:/Workspace/LiteEngine/core/resources/filament/editor/materials/wireframe.filamat");
    wireframeSystem->setWireframeColor(glm::vec4(1.0f, 1.0f, 1.0f, 0.6f));
    wireframeSystem->attachTo(currentScene);
    currentScene->addSystem(wireframeSystem.get());

    /*----------------------------------------------------------------------------
    SETUP CAMERA INITIAL POSITION
    ----------------------------------------------------------------------------*/
    glm::vec3 center(0, 0, 0);
    float radius = 5.0f;
    glm::vec3 offsetEye    = center + glm::vec3(0, radius * 0.3f, radius);
    glm::vec3 offsetCenter = glm::normalize(center - offsetEye);

    sceneRenderer.setCameraState(offsetEye, offsetEye + offsetCenter);

    auto importer = std::make_unique<AssimpImporter>();

    /*----------------------------------------------------------------------------
    SETUP SELECT OBJECT SYSTEM
    ----------------------------------------------------------------------------*/

    std::unique_ptr<FilamentObjectSelectorSystem> objectSelector = std::make_unique<FilamentObjectSelectorSystem>();
    objectSelector->setCamera(sceneRenderer.getCurrentCamera());
    objectSelector->attachTo(sceneRenderer.getScene());
    currentScene->addSystem(objectSelector.get());

    /*----------------------------------------------------------------------------
    SETUP GIZMO SYSTEM
    O gizmo é desenhado num overlay próprio (cena + view separadas), composto por
    cima da cena 3D dentro do mesmo frame — por isso não é ocluído pela geometria
    nem aparece no picking da cena principal.
    Os recursos GPU (cena/view/factory/root) são criados de forma preguiçosa no
    primeiro hook, já na render thread; aqui só se injetam dependências.
    Registro via addSystem ANTES de sceneRenderer.start() — regra dos systems.

    TODO: preencher os 9 modelos em `gizmoParts` (move/rotate/scale × X/Y/Z).
          Enquanto os slots estiverem vazios o overlay é criado e desenhado vazio.
    ----------------------------------------------------------------------------*/
    
    GizmoParts gizmo = lite::gizmo::buildGizmoParts(
        importer.get(),
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
    std::unique_ptr<lite::FilamentGizmoSystem> gizmoSystem =
        std::make_unique<lite::FilamentGizmoSystem>(
            FilamentUtils::getEngine(),
            std::move(gizmo)
        );
    gizmoSystem->setCamera(sceneRenderer.getCurrentCamera());
    gizmoSystem->attachTo(currentScene);
    currentScene->addSystem(gizmoSystem.get());

    /*---------------------------------------------------------------------------*/
    //GIZMO VALUES
    /*---------------------------------------------------------------------------*/

    std::optional<GizmoAction> gizmoAction = std::nullopt;
    
    glm::vec3 startingGizmoLocation = glm::vec3(0, 0, 0);
    
    float startingDraggingDistance = 0;
    glm::vec3 startingDragginObjectLocation = glm::vec3(0, 0, 0);

    float startingTurningAngle = 0;
    glm::quat startingTurningObjectRotation = glm::quat(0, 0, 0, 0);

    float startingScaleFactorDistance = 0;
    glm::vec3 startingSizeObjectScale = glm::vec3(1, 1, 1);
    
    glm::vec2 lastMousePosition = glm::vec2(0, 0);

    /*---------------------------------------------------------------------------*/
    //END || GIZMO VALUES
    /*---------------------------------------------------------------------------*/
    
    /*----------------------------------------------------------------------------
    SETUP SERIALIZER
    ----------------------------------------------------------------------------*/
    SceneSerializer* sceneSerialzer = new FlatBuffersSceneSerializer();
    SceneDTOMapper<
        FilamentAsset3dInstance,
        FilamentAsset3dTransform,
        FilamentInstanceFactory,
        CEF_Filament_UIRendererThreaded>* sceneMapper = new DummySceneDTOMapper();

    sceneMapper->registerMapper( new DummyAsset3dDTOMapper() );
    
    
    /*----------------------------------------------------------------------------
    SETUP UI RENDERER (CEF)
    ----------------------------------------------------------------------------*/
    lite::UIInstance<CEF_Filament_UIRendererThreaded>* uiInstance = new lite::CEF_Filament_UIInstance(uiRenderer);

    UIPanelElement<CEF_Filament_UIRendererThreaded>* root = nullptr;
    if (!(root = uiInstance->start())) {
        std::cerr << "Failed to start UIRenderer" << std::endl;
        return -1;
    }

    CEF_UIPanelElement* leftPanel = new CEF_UIPanelElement(uiRenderer);
    root->addChildComponent(leftPanel, 0, 0);

    UITextElement<CEF_Filament_UIRendererThreaded>* uiText = new CEF_UITextElement(uiRenderer);
    leftPanel->addChildComponent(uiText, 0, 0, 1, 2);

    UITextInputElement<CEF_Filament_UIRendererThreaded>* uiInput = new CEF_UITextInputElement(uiRenderer, "Modelo");
    leftPanel->addChildComponent(uiInput, 1, 0, 1, 2);


    UIButtonElement<CEF_Filament_UIRendererThreaded>* loadModelButton = new CEF_UIButtonElement(uiRenderer, "Carregar");
    loadModelButton->registerEvent("click", [&](CEF_Filament_UIRendererThreaded*, int, std::string) {
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

    UIButtonElement<CEF_Filament_UIRendererThreaded>* deleteModelButton = new CEF_UIButtonElement(uiRenderer, "Deletar");
    deleteModelButton->registerEvent("click", [&](CEF_Filament_UIRendererThreaded*, int, std::string) {
        // if (currentScene->destroy(currentInstanceId)) {
        //     std::cout << "current model deleted" << std::endl;
        // } else {
        //     std::cout << "something went wrong on deletion" << std::endl;
        // }
    });

    UIButtonElement<CEF_Filament_UIRendererThreaded>* saveSceneButton = new CEF_UIButtonElement(uiRenderer, "Salvar");
    saveSceneButton->registerEvent("click",  [&](CEF_Filament_UIRendererThreaded*, int, std::string){

        SceneDTO sceneDto = sceneMapper->toDto(currentScene);
        sceneSerialzer->save(sceneDto, SCENE_PATH);

    });

    leftPanel->addChildComponent(loadModelButton, 2, 0);
    leftPanel->addChildComponent(deleteModelButton, 2, 1);
    leftPanel->addChildComponent(saveSceneButton, 3, 0);

    root->draw();

    /*----------------------------------------------------------------------------
    START RENDER LOOP
    UI (update/render) é responsabilidade interna da Scene; a limpeza de
    wireframes de assets deletados é do FilamentWireframeSystem (onFrameEnd).
    Commands (IBL, lights) são processados antes do primeiro frame.
    ----------------------------------------------------------------------------*/
    sceneRenderer.start();

    /*----------------------------------------------------------------------------
    LOAD INITIAL ASSET (after start — render thread will instantiate via update)
    ----------------------------------------------------------------------------*/
    
    Asset3dData rootNode;
    std::vector<std::unique_ptr<MaterialData>> materials;
    int currentInstanceId = -1;
    FilamentAsset3dInstance* assetPtr = nullptr;

    if (importer->import(
        // "D:/Workspace/LiteEngine/test-resources/simple_sphere.fbx"
        "C:/Users/pixqu/Downloads/Jason Stalhart/Base_Mesh/Aiden_Stallhart_BaseMesh_skeleton_Ver1.fbx"
        , rootNode, materials)) {
        currentInstanceId = currentScene->create(
            rootNode,
            materials,
            TransformUtils<FilamentAsset3dTransform>::build(),
            true
        );
        assetPtr = currentScene->get(currentInstanceId);
    }

    std::cout << "Starting main loop..." << std::endl;

    /*----------------------------------------------------------------------------
    MAIN LOOP
    ----------------------------------------------------------------------------*/
    bool running = true;

    bool    mov_front = false
        ,   mov_back = false
        ,   mov_right = false
        ,   mov_left = false
        ,   mov_up = false
        ,   mov_down = false
        ,   mov_active = false
        
        ,   multiple_selection_active = false;

    bool start_object_query_select = false;
    const float VELOCITY_MOVEMENT = radius * 50.f;
    const float VELOCITY_LOOK = 0.003f;
    float horizontal_direction = 0, vertical_direction = 0;

    
    //FIXME: ESSE SISTEMA ESTÁ DEPENDENDO DO SCENERENDERER. COMO SCENERENEDERER TEM MUITOS TEMPLATES, É MELHOR NÃO PUXAR ESSA DEPENDÊMCIA PARA O NAVIGATIONSYSTEM
    // TAMBÉM O MÉTODO SETCAMERASTATE É UMA COISA ESPECÍFICA DA FIALMENT. O MELHOR SERIA NÃO CHAMAR ESSE MÉTODO EXPLICITAMENTE E ISSO SER IMPLEMENTADO DE FORMA 
    // TRANSPARENTE POR UMA HERANÇA DO TRANSFORM
    EditorNavigationSystem* navigation = new EditorNavigationSystem(
        [&](glm::vec3 center, glm::vec3 offsetCenter, glm::vec3 offsetEye){
            sceneRenderer.setCameraState(offsetEye, offsetEye + offsetCenter);

            return  sceneRenderer   
                    .getCurrentCamera()
                    ->getTransform()
                    ->getPosition();
    });
    sceneRenderer.getScene()->addSystem(navigation);
    //TODO: CHAMAR SCENE CONFIGURER AQUI DEPOIS DE INSTANCIADO TODOS OS SISTEMAS
    
    while (running) {
        SDL_Event ev;
        lite::InputEvent inputEvent;

        while (SDL_PollEvent(&ev)) {
            navigation->digestInputEvent(ev);

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

        if(inputEvent.keys.contains(lite::INPUT_KEYS::MOUSE_RIGHT))
        {
            mov_active = inputEvent.keys[lite::INPUT_KEYS::MOUSE_RIGHT] == lite::INPUT_KEY_STATES::DOWN;
        }

        glm::vec3 currentCamLocation = sceneRenderer   
                                    .getCurrentCamera()
                                    ->getTransform()
                                    ->getPosition();
        
        // TODO: CALCULAR ISSO FORA DO CLIQUE PODE SER "CARO", REVER ISSO
        // // Posição do clique capturada no evento (ev já pode ser outro evento
        // // do mesmo frame); viewport real da janela (FULLSCREEN_DESKTOP =
        // // resolução do desktop, não SCREEN_WIDTH/HEIGHT)
        int winW = 0, winH = 0;
        SDL_GetWindowSize(window, &winW, &winH);
        glm::vec3 cameraRayDirection = objectSelector->getCameraRay(
                    lastMousePosition,
                    glm::vec2(winW, winH),
                    10000.0f
                );

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
                
                // Meio-ângulo do cone de broad-phase = metade da abertura de lente
                // (FOV vertical) da câmera.
                float coneHalfAngle = sceneRenderer.getCurrentCamera()->getFieldOfViewInRadians() * 0.5f;
    
                //------------------------------------------------------------------------------------------------
                //ENCONTRA PARTE DO GIZMO
                //------------------------------------------------------------------------------------------------
                
                if(!gizmoAction.has_value())
                {
                    gizmoAction = gizmoSystem->intersectGizmo(
                        currentCamLocation,
                        cameraRayDirection,
                        coneHalfAngle
                    );
    
                    if(gizmoAction.has_value())
                    {
                        GizmoAction currentGizmoAction = gizmoAction.value();
                        
                        startingGizmoLocation = gizmoSystem->getGizmoTransform()->getPosition();
                        
                        
                        startingDraggingDistance = gizmoSystem->getDistanceOnAxis(
                                axisOf(gizmoAction.value())
                            ,   currentCamLocation
                            ,   cameraRayDirection
                            ,   startingGizmoLocation
                        );
                        startingDragginObjectLocation = assetPtr->getTransform()->getPosition();
                        
                        startingTurningAngle = gizmoSystem->getAngleOnAxisOnDegrees(
                                axisOf(gizmoAction.value())
                            ,   currentCamLocation
                            ,   cameraRayDirection
                            ,   startingGizmoLocation
                        );
                        startingTurningObjectRotation = assetPtr->getTransform()->getRotation();

                        startingScaleFactorDistance = startingDraggingDistance; 
                        startingSizeObjectScale = assetPtr->getTransform()->getScale();
                
                    }
                }
    
                //------------------------------------------------------------------------------------------------
                //END || ENCONTRA PARTE DO GIZMO
                //------------------------------------------------------------------------------------------------
    
                if(!gizmoAction.has_value())
                {
                    //------------------------------------------------------------------------------------------------
                    //ENCONTRA OBJETO NA CENA
                    //------------------------------------------------------------------------------------------------
        
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
                        sceneRenderer.postCommand([wireframe = wireframeSystem.get(), objectFound, multiple_selection_active]() {
                            
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
            //------------------------------------------------------------------------------------------------
            // CLICK RELEASED
            //------------------------------------------------------------------------------------------------
            else
            {
                gizmoAction = std::nullopt;
            }
            //------------------------------------------------------------------------------------------------
            // END || CLICK RELEASED
            //------------------------------------------------------------------------------------------------
        }
        //------------------------------------------------------------------------------------------------
        //END || CLICK TOGGLE
        //------------------------------------------------------------------------------------------------

        if(gizmoAction.has_value())
        {
            
            if( gizmoAction == GizmoAction::MOVE_X || 
                gizmoAction == GizmoAction::MOVE_Y || 
                gizmoAction == GizmoAction::MOVE_Z)
            {
                float draggingDistance =
                gizmoSystem->getDistanceOnAxis(
                    axisOf(gizmoAction.value())
                    ,   currentCamLocation
                    ,   cameraRayDirection
                    ,   startingGizmoLocation
                ) - startingDraggingDistance;
                
                glm::vec3 dealocated = glm::vec3(
                    startingDragginObjectLocation.x,
                    startingDragginObjectLocation.y,
                    startingDragginObjectLocation.z
                );
    
                switch(gizmoAction.value()){
                    case GizmoAction::MOVE_X:{
                        dealocated.x += draggingDistance;
                    }break;
                    case GizmoAction::MOVE_Y:{
                        dealocated.y += draggingDistance;
                    }break;
                    case GizmoAction::MOVE_Z:{
                        dealocated.z += draggingDistance;
                    }break;
                }
                
                assetPtr->getTransform()->setPosition(dealocated);
                
                std::cout << std::format("Dragging gizmo: {} distance: {}", toString(gizmoAction.value()), draggingDistance) << std::endl;
            }
            else if(
                gizmoAction == GizmoAction::ROTATE_X || 
                gizmoAction == GizmoAction::ROTATE_Y || 
                gizmoAction == GizmoAction::ROTATE_Z
            )
            {
                float turningAngle =
                gizmoSystem->getAngleOnAxisOnDegrees(
                        axisOf(gizmoAction.value())
                    ,   currentCamLocation
                    ,   cameraRayDirection
                    ,   startingGizmoLocation
                ) - startingTurningAngle;

                // NaN = ângulo inválido (raio paralelo ao plano / cursor no
                // centro) — pula o frame para não propagar NaN à rotação.
                if (!std::isnan(turningAngle))
                {
                    // NÃO somar o ângulo a componentes do quatérnion: quatérnion
                    // não é (θx, θy, θz). Monta-se o DELTA de rotação com
                    // angleAxis (quatérnion unitário) e compõe-se com a rotação
                    // inicial. Somar graus a q.x/y/z produz um quatérnion NÃO
                    // unitário → mat4_cast o interpreta como rotação * |q|² →
                    // vira escala (estica e some). Ver memória do projeto.
                    glm::quat delta = glm::angleAxis(
                        glm::radians(turningAngle), 
                        axisOf(gizmoAction.value()));
                    glm::quat rotated = delta * startingTurningObjectRotation;

                    assetPtr->getTransform()->setRotation(rotated, true); // world-space

                    std::cout << std::format("Rotating gizmo: {} angle: {}", toString(gizmoAction.value()), turningAngle) << std::endl;
                }
            }
            else if(
                gizmoAction == GizmoAction::SCALE_X || 
                gizmoAction == GizmoAction::SCALE_Y || 
                gizmoAction == GizmoAction::SCALE_Z
            )
            {
                float scalingDistance =
                gizmoSystem->getDistanceOnAxis(
                    axisOf(gizmoAction.value())
                    ,   currentCamLocation
                    ,   cameraRayDirection
                    ,   startingGizmoLocation
                ) - startingScaleFactorDistance;

                glm::vec3 scaled = glm::vec3(
                    startingSizeObjectScale.x,
                    startingSizeObjectScale.y,
                    startingSizeObjectScale.z
                );

                switch(gizmoAction.value()){
                    case GizmoAction::SCALE_X:{
                        scaled.x += scalingDistance / startingScaleFactorDistance;
                    }break;
                    case GizmoAction::SCALE_Y:{
                        scaled.y += scalingDistance / startingScaleFactorDistance;
                    }break;
                    case GizmoAction::SCALE_Z:{
                        scaled.z += scalingDistance / startingScaleFactorDistance;
                    }break;
                }

                assetPtr->getTransform()->setScale(scaled);

                std::cout << std::format("Scaling gizmo: {} distance: {}", toString(gizmoAction.value()), scalingDistance) << std::endl;
            }

        }
    }

    std::cout << "Shutting down..." << std::endl;

    /*----------------------------------------------------------------------------
    CLEANUP
    O destrutor do wireframe toca GPU → destruição vai para a render thread via
    postCommand (removendo do scene no mesmo comando, antes do reset). O stop()
    explícito garante que o comando é drenado antes dos locais da main morrerem;
    o stop() do destrutor do sceneRenderer vira no-op (idempotente).
    ----------------------------------------------------------------------------*/
    if (currentInstanceId >= 0) {
        currentScene->destroy(currentInstanceId);   // por id — a Scene é a dona
    }

    sceneRenderer.postCommand([&]() {
        currentScene->removeSystem(wireframeSystem.get());
        wireframeSystem.reset();

        // Mesmo motivo: o destrutor do gizmo destrói view/scene/factory do
        // overlay (recursos do Engine) — tem de rodar na render thread.
        currentScene->removeSystem(gizmoSystem.get());
        gizmoSystem.reset();
    });
    sceneRenderer.stop();

    importer.reset();

    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Cleanup complete" << std::endl;

    return 0;
}
