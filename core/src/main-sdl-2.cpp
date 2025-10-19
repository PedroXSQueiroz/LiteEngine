#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <SDL_syswm.h>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include <math/vec3.h>
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <filament/Camera.h>
#include <filament/LightManager.h>
#include <filament/Skybox.h>
#include <filament/IndirectLight.h>
#include <filament/Viewport.h>
#include <filamentapp/IBL.h>
#include <camutils/Manipulator.h>

#include <utils/EntityManager.h>
#include <utils/Path.h>
#include <utils/Panic.h>

using namespace filament;
using namespace camutils;
using namespace utils;
using namespace std;

// Retorna o HWND nativo da janela SDL (Windows)
void* getNativeWindow(SDL_Window* sdlWindow) {
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(sdlWindow, &wmi)) {
        return nullptr;
    }
    return (void*) wmi.info.win.window;
}

// Carrega IBL usando filamentapp::IBL e retorna o unique_ptr para mantenção de lifetime.
// Se falhar retorna nullptr.
static unique_ptr<IBL> loadIBLUnique(Engine* engine, const std::string& path, Scene* scene) {
    Path iblPath(path);
    if (!iblPath.exists()) {
        std::cerr << "IBL path does not exist: " << path << std::endl;
        return nullptr;
    }

    auto ibl = make_unique<IBL>(*engine);
    bool ok = false;
    if (!iblPath.isDirectory()) {
        ok = ibl->loadFromEquirect(iblPath);
    } else {
        ok = ibl->loadFromDirectory(iblPath);
    }
    if (!ok) {
        std::cerr << "Failed to load IBL from: " << path << std::endl;
        return nullptr;
    }

    // Set skybox and indirect light on scene — note: these are owned by IBL, so IBL must live.
    scene->setSkybox(ibl->getSkybox());
    scene->setIndirectLight(ibl->getIndirectLight());
    return ibl;
}

int main(int /*argc*/, char** /*argv*/) {
    const int SCREEN_WIDTH = 1080;
    const int SCREEN_HEIGHT = 720;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Request core profile OpenGL (adjust if your GPU/driver needs other version)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow(
        "Filament + SDL2 IBL Example",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // // Crie o contexto OpenGL *antes* de criar o Filament SwapChain
    // SDL_GLContext glContext = SDL_GL_CreateContext(window);
    // if (!glContext) {
    //     std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
    //     SDL_DestroyWindow(window);
    //     SDL_Quit();
    //     return -1;
    // }
    // // Assegura que o contexto está current
    // if (SDL_GL_MakeCurrent(window, glContext) != 0) {
    //     std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
    //     SDL_GL_DeleteContext(glContext);
    //     SDL_DestroyWindow(window);
    //     SDL_Quit();
    //     return -1;
    // }

    // --- Inicializa Filament ---
    Engine* engine = Engine::create(Engine::Backend::OPENGL);
    if (!engine) {
        std::cerr << "Failed to create Filament Engine" << std::endl;
        // SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Agora crie o SwapChain passando o HWND (Windows). OBS: passe o pointer retornado por getNativeWindow.
    void* nativeWindow = getNativeWindow(window);
    if (!nativeWindow) {
        std::cerr << "Failed to get native window handle" << std::endl;
        Engine::destroy(&engine);
        // SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SwapChain* swapChain = engine->createSwapChain(nativeWindow, SwapChain::CONFIG_HAS_STENCIL_BUFFER);
    Renderer* renderer = engine->createRenderer();
    Scene* scene = engine->createScene();
    View* view = engine->createView();

    // Camera
    Entity cameraEntity = EntityManager::get().create();
    Camera* camera = engine->createCamera(cameraEntity);
    camera->setProjection(45.0f, float(SCREEN_WIDTH) / float(SCREEN_HEIGHT), 0.1f, 100.0f, Camera::Fov::VERTICAL);
    camera->lookAt({0, 0, 3}, {0, 0, 0}, {0, 1, 0});

    filament::camutils::Manipulator<float>* cameraMan = filament::camutils::Manipulator<float>::Builder()
        .targetPosition(0, 0, -4)
        .flightMoveDamping(15.0)
        .build(filament::camutils::Mode::ORBIT);

    view->setCamera(camera);
    view->setScene(scene);
    view->setViewport({0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
    // cameraMan->setViewport(view->getViewport());

    // Simple directional light
    Entity lightEntity = EntityManager::get().create();
    LightManager::Builder(LightManager::Type::SUN)
        .color({1.0f, 1.0f, 0.95f})
        .intensity(30000.0f)
        .direction({0.0f, -1.0f, -0.5f})
        .castShadows(true)
        .build(*engine, lightEntity);
    scene->addEntity(lightEntity);

    // --- Carrega IBL e mantém o objeto vivo aqui ---
    // Ajuste o path conforme seu layout. Ex.: a pasta que contém arquivos gerados pelo Filament (ktx)
    std::string iblPath = "D:/Workspace/LiteEngine/3rd_party/filament/out/samples/assets/ibl/lightroom_14b";
    // std::string iblPath = "D:/Workspace/LiteEngine/3rd_party/filament/out/samples/assets/ibl/external_night";
    
    std::unique_ptr<IBL> ibl = loadIBLUnique(engine, iblPath, scene);
    if (!ibl) {
        std::cerr << "Warning: IBL failed to load. Scene may be dark." << std::endl;
    } else {
        std::cout << "IBL loaded and applied to scene." << std::endl;
    }

    // Loop principal
    bool running = true;

    int mouseX = 0, mouseY = 0;
    

    SDL_GetMouseState(&mouseX, &mouseY);

    filament::math::vec3<float> camPosition(0, 0, 3);
    filament::math::vec3<float> camDirection(0, 0, 0);
    filament::math::vec3<float> camUpDirection(0, 1, 0);

    cameraMan->grabBegin(mouseX, mouseY, false);

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            
            
            if (ev.type == SDL_QUIT) running = false;
            // if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                //     int w = ev.window.data1;
                //     int h = ev.window.data2;
                //     view->setViewport({0, 0, (uint32_t)w, (uint32_t)h});
                // }
                
            if(ev.type == SDL_MOUSEMOTION)
            {
                int previewsMouseX = mouseX;
                int previewsMouseY = mouseY;
    
                SDL_GetMouseState(&mouseX, &mouseY);
    
                int dealocationX = mouseX - previewsMouseX;
                int dealocationY = mouseY - previewsMouseY;
                std::cout << std::printf("Mouse noved x: %i, y %i position: %i, %i", 
                    dealocationX, dealocationY,
                    mouseX, mouseY
                    ) << std::endl;

                // camDirection.x += dealocationX;
                // camDirection.z += dealocationY;
                
                cameraMan->grabUpdate(ev.motion.x, ev.motion.y);
                
                filament::math::float3 eye, center, up;
                cameraMan->getLookAt(&eye, &center, &up);
                
                // camera->lookAt(camDirection, camPosition, camUpDirection);
                camera->lookAt(eye, center, up);
            }
            else
            {
                std::cout << std::printf("Mouse stopped, position x: %i, y %i", mouseX, mouseY) << std::endl;
            }
            
        }

        if (renderer->beginFrame(swapChain)) {
            renderer->render(view);
            renderer->endFrame();
        }

        // Em caso de renderização com Filament+SDL+GL we usually don't call SDL_GL_SwapWindow,
        // because Filament takes care of presenting. However Filament's OpenGL backend may
        // expect you to swap if SwapChain mapping requires it. If you see flicker, try toggling.
        SDL_GL_SwapWindow(window);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    cameraMan->grabEnd();

    // Cleanup (na ordem inversa)
    scene->remove(lightEntity);
    engine->destroy(lightEntity);

    engine->destroy(view);
    engine->destroy(scene);
    engine->destroy(renderer);
    engine->destroy(swapChain);
    engine->destroyCameraComponent(cameraEntity);
    EntityManager::get().destroy(cameraEntity);

    // ensure we keep ibl alive until now (unique_ptr destructs here)

    Engine::destroy(&engine);

    // SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
