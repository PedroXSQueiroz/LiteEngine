//#define _HAS_ITERATOR_DEBUGGING 0
//#define _ITERATOR_DEBUG_LEVEL 0

#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <filament/Camera.h>
#include <filament/LightManager.h>
#include <utils/EntityManager.h>
#include <utils/Entity.h>

#include <SDL3/SDL.h>
#include <iostream>

using namespace filament;
using namespace utils;

int main(char* args[])
{
    SDL_Log("Starting SDL...\n");
    
    // Inicializa o SDL com suporte a vídeo
    int sdl_init_result = SDL_Init(SDL_INIT_VIDEO);
    if (sdl_init_result != 0) {
        std::cerr << "Erro ao inicializar SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Cria a janela SDL com contexto OpenGL
    SDL_Window* window = SDL_CreateWindow(
        "Filament + SDL2",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    if (!window) {
        std::cerr << "Erro ao criar janela SDL: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Cria o contexto OpenGL
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Erro ao criar contexto OpenGL: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Inicializa o Filament Engine
    Engine* engine = Engine::create(Engine::Backend::OPENGL);
    if (!engine) {
        std::cerr << "Falha ao criar Engine Filament" << std::endl;
        // SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Cria o swap chain a partir da janela SDL
    SwapChain* swapChain = engine->createSwapChain((void*)window);
    Renderer* renderer = engine->createRenderer();
    Scene* scene = engine->createScene();
    View* view = engine->createView();

    // Configura câmera
    Entity cameraEntity = EntityManager::get().create();
    Camera* camera = engine->createCamera(cameraEntity);
    camera->setProjection(45.0f, 1280.0f / 720.0f, 0.1f, 10.0f, Camera::Fov::VERTICAL);
    camera->lookAt({0, 0, 3}, {0, 0, 0});

    view->setCamera(camera);
    view->setScene(scene);
    // view->setColorGrading({0.2f, 0.2f, 0.25f, 1.0f});

    // Luz direcional simples
    Entity lightEntity = EntityManager::get().create();
    LightManager::Builder(LightManager::Type::SUN)
        .color({1.0f, 1.0f, 0.9f})
        .intensity(50000.0f)
        .direction({0.0f, -1.0f, -0.5f})
        .castShadows(true)
        .build(*engine, lightEntity);
    scene->addEntity(lightEntity);

    // Loop principal
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }

        if (renderer->beginFrame(swapChain)) {
            renderer->render(view);
            renderer->endFrame();
        }

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    engine->destroy(lightEntity);
    engine->destroyCameraComponent(cameraEntity);
    EntityManager::get().destroy(cameraEntity);

    engine->destroy(view);
    engine->destroy(scene);
    engine->destroy(renderer);
    engine->destroy(swapChain);
    Engine::destroy(&engine);

    // SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}