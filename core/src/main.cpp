#include <memory.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

#include <SDL.h>
#include <SDL_syswm.h>
#define SDL_MAIN_HANDLED

#include <filament/Engine.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Scene.h>
#include <filament/Viewport.h>
#include <filament/Renderer.h>
#include <camutils/Manipulator.h>
#include <utils/EntityManager.h>
#include <utils/NameComponentManager.h>
#include <filament/LightManager.h>
#include <filamentapp/IBL.h>
#include <filament/IndirectLight.h>
#include <filament/Camera.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/FilamentInstance.h>
#include <gltfio/MaterialProvider.h>
#include <filament/MaterialInstance.h>
#include <gltfio/TextureProvider.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/ResourceLoader.h>
#include <uberarchive.h>
#include <utils/Path.h>
#include <math/vec3.h>
#include <math/mat4.h>

#if defined(_WIN32)
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif

using namespace std;

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

static std::ifstream::pos_type getFileSize(const char* filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

static unique_ptr<IBL> loadIBLUnique(filament::Engine* engine, const std::string& path, filament::Scene* scene) {
    utils::Path iblPath(path);
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

    scene->setSkybox(ibl->getSkybox());
    scene->setIndirectLight(ibl->getIndirectLight());
    return ibl;
}

void loadResources(
    filament::Engine* engine, 
    filament::gltfio::FilamentAsset* asset, 
    filament::gltfio::FilamentInstance* instance, 
    const utils::Path& filename
) {
    std::string const gltfPath = filename.getAbsolutePath();
    filament::gltfio::ResourceConfiguration configuration = {};
    configuration.engine = engine;
    configuration.gltfPath = gltfPath.c_str();
    configuration.normalizeSkinningWeights = true;

    filament::gltfio::ResourceLoader* resourceLoader = new filament::gltfio::ResourceLoader(configuration);
    resourceLoader->addTextureProvider("image/png", filament::gltfio::createStbProvider(engine));
    resourceLoader->addTextureProvider("image/jpeg", filament::gltfio::createStbProvider(engine));
    resourceLoader->addTextureProvider("image/ktx2", filament::gltfio::createKtx2Provider(engine));
    
    resourceLoader->loadResources(asset);
    resourceLoader->asyncUpdateLoad();

    asset->releaseSourceData();

    const size_t matInstanceCount = instance->getMaterialInstanceCount();
    filament::MaterialInstance* const* const instances = instance->getMaterialInstances();
    for (int mi = 0; mi < matInstanceCount; mi++) {
        instances[mi]->setStencilWrite(true);
        instances[mi]->setStencilOpDepthStencilPass(filament::MaterialInstance::StencilOperation::INCR);
    }
}

filament::gltfio::FilamentInstance* loadAsset(
    filament::Engine* engine, 
    filament::Scene* scene, 
    filament::gltfio::FilamentAsset* &assetOut, 
    const utils::Path& filename
) {
    long const contentSize = static_cast<long>(getFileSize(filename.c_str()));
    if (contentSize <= 0) {
        std::cerr << "Unable to open " << filename << std::endl;
        exit(1);
    }

    std::ifstream in(filename.c_str(), std::ifstream::binary | std::ifstream::in);
    std::vector<uint8_t> buffer(static_cast<unsigned long>(contentSize));
    if (!in.read((char*) buffer.data(), contentSize)) {
        std::cerr << "Unable to read " << filename << std::endl;
        exit(1);
    }

    filament::gltfio::MaterialProvider* matProv = filament::gltfio::createUbershaderProvider(engine, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);
    filament::gltfio::AssetLoader* assetLoader = filament::gltfio::AssetLoader::create({ 
        engine,
        matProv,
        new utils::NameComponentManager(utils::EntityManager::get())
    });

    filament::gltfio::FilamentAsset* asset = assetLoader->createAsset(buffer.data(), buffer.size());
    if (!asset) {
        std::cerr << "Unable to parse " << filename << std::endl;
        exit(1);
    }

    filament::gltfio::FilamentInstance* instance = asset->getInstance();
    scene->addEntities(asset->getEntities(), asset->getEntityCount());
    
    std::cout << "Asset loaded: " << asset->getEntityCount() << " entities" << std::endl;
    
    buffer.clear();
    buffer.shrink_to_fit();

    assetOut = asset;
    return instance;
}

int main(int /*argc*/, char** /*argv*/){
    
    /*----------------------------------------------------------------------------
    SETUP WINDOW
    ----------------------------------------------------------------------------*/
    const int SCREEN_WIDTH = 1980;
    const int SCREEN_HEIGHT = 1080;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);   // obrigatório!!
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);    // depth buffer
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // Criar janela SEM OpenGL - Filament gerenciará isso
    SDL_Window* window = SDL_CreateWindow(
        "Lite", 
        SDL_WINDOWPOS_CENTERED, 
        SDL_WINDOWPOS_CENTERED, 
        SCREEN_WIDTH, 
        SCREEN_HEIGHT, 
        SDL_WINDOW_ALLOW_HIGHDPI |
        SDL_WINDOW_SHOWN |
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    /*----------------------------------------------------------------------------
    SETUP RENDERER ENGINE
    ----------------------------------------------------------------------------*/
    void* nativeWindow = getNativeWindowHandle(window);
    if (!nativeWindow) {
        std::cerr << "Failed to get native window handle" << std::endl;
        return -1;
    }
    
    std::cout << "Creating Filament engine..." << std::endl;
    
    // Criar engine
    filament::Engine* engine = filament::Engine::Builder()
        .backend(filament::Engine::Backend::OPENGL)
        .build();

    if (!engine) {
        std::cerr << "Failed to create Filament engine" << std::endl;
        return -1;
    }
    
    std::cout << "Engine created successfully" << std::endl;

    // Criar swap chain
    filament::SwapChain* swapChain = engine->createSwapChain(nativeWindow);
    if (!swapChain) {
        std::cerr << "createSwapChain returned null." << std::endl;
        return -1;
    }
    
    std::cout << "SwapChain created successfully" << std::endl;

    // Criar renderer
    filament::Renderer* renderer = engine->createRenderer();
    if (!renderer) {
        std::cerr << "createRenderer returned null." << std::endl;
        return -1;
    }
    
    std::cout << "Renderer created successfully" << std::endl;
    
    /*----------------------------------------------------------------------------
    SETUP ENTITIES CONTAINER
    ----------------------------------------------------------------------------*/
    utils::EntityManager& em = utils::EntityManager::get();

    /*----------------------------------------------------------------------------
    SETUP SCENE
    ----------------------------------------------------------------------------*/
    utils::Entity cameraEntity = em.create();
    filament::Camera* camera = engine->createCamera(cameraEntity);
    
    filament::Scene* scene = engine->createScene();
    filament::View* view = engine->createView();

    // Configurar view
    view->setCamera(camera);
    view->setScene(scene);
    view->setPostProcessingEnabled(false); // Desabilitar por enquanto para debug
    
    int fbW = SCREEN_WIDTH, fbH = SCREEN_HEIGHT;
    SDL_GetWindowSize(window, &fbW, &fbH);
    view->setViewport({0, 0, (uint32_t)fbW, (uint32_t)fbH});
    // view->setClearTargets(true, true); // habilita clear color + depth
    // view->setClearColor({0.05f, 0.05f, 0.05f, 1.0f}); // alpha = 1.0 importante
    
    // Configurar câmera DEPOIS de criar view
    camera->setProjection(45.0f, float(fbW) / float(fbH), 0.1f, 2000.0f);
    
    std::cout << "Scene and View created successfully" << std::endl;

    /*----------------------------------------------------------------------------
    LOAD SCENE ENTITIES
    ----------------------------------------------------------------------------*/
    std::cout << "Loading GLTF asset..." << std::endl;
    
    filament::gltfio::FilamentAsset* asset = nullptr;
    filament::gltfio::FilamentInstance* gltfInstance = loadAsset(
        engine, 
        scene, 
        asset, 
        "C:/Users/pixqu/Downloads/coast_african_rock_m_17_wghiado_raw.glb"
    );
    
    if(asset && gltfInstance)
    {
        std::cout << "Loading resources..." << std::endl;
        loadResources(
            engine, 
            asset, 
            gltfInstance, 
            "C:/Users/pixqu/Downloads/coast_african_rock_m_17_wghiado_raw.glb"
        );
        std::cout << "Resources loaded successfully" << std::endl;
    } else {
        std::cerr << "Failed to load GLTF asset!" << std::endl;
    }

    // Calcular bounding box do asset para posicionar câmera
    filament::math::float3 center(0, 0, 0);
    float radius = 5.0f;
    
    if (asset) {
        auto bbox = asset->getBoundingBox();
        center = bbox.center();
        radius = length(bbox.extent()) * 1.5f;
        std::cout << "Asset center: " << center.x << ", " << center.y << ", " << center.z << std::endl;
        std::cout << "Asset radius: " << radius << std::endl;
    }

    //SETUP CAMERA - posicionar olhando para o modelo
    filament::math::float3 offsetEye = center + filament::math::float3(0, radius * 0.3f, radius);
    filament::math::float3 offsetCenter = normalize(center - offsetEye);
    
    //SETUP LIGHTNING IBL
    std::string iblPath = "D:/Workspace/LiteEngine/3rd_party/filament/out/samples/assets/ibl/lightroom_14b";
    
    std::unique_ptr<IBL> ibl = loadIBLUnique(engine, iblPath, scene);
    if (!ibl) {
        std::cerr << "Warning: IBL failed to load. Scene may be dark." << std::endl;
    } else {
        std::cout << "IBL loaded and applied to scene." << std::endl;
        scene->getIndirectLight()->setIntensity(30000.0f);
    }

    // Simple directional light
    utils::Entity lightEntity = utils::EntityManager::get().create();
    filament::LightManager::Builder(filament::LightManager::Type::SUN)
        .color({1.0f, 1.0f, 0.95f})
        .intensity(100000.0f)
        .direction({0.6f, -1.0f, -0.8f})
        .castShadows(false) // Desabilitar sombras por enquanto
        .build(*engine, lightEntity);
    scene->addEntity(lightEntity);
    
    std::cout << "Lighting setup complete" << std::endl;
    std::cout << "Starting main loop..." << std::endl;
    
    /*----------------------------------------------------------------------------
    MAIN LOOP
    ----------------------------------------------------------------------------*/
    
    bool running = true;
    Uint64 prevTicks = SDL_GetPerformanceCounter();
    
    //NAVIGATION
    bool mov_front = false, mov_back = false, mov_right = false, mov_left = false, mov_up = false, mov_down = false;
    const float VELOCITY_MOVEMENT = radius * 0.5f; // Ajustar baseado no tamanho do modelo
    const float VELOCITY_LOOK = 0.003f;
    float horizontal_direction = 0, vertical_direction = 0;
    
    // Capturar mouse
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    int frameCount = 0;
    
    while(running)
    {
        Uint64 now = SDL_GetPerformanceCounter();
        float deltaTime = (float)((now - prevTicks) / (double)SDL_GetPerformanceFrequency());
        prevTicks = now;
        
        SDL_Event ev;

        while (SDL_PollEvent(&ev)) {
            switch (ev.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_MOUSEMOTION:{
                horizontal_direction += ev.motion.xrel * VELOCITY_LOOK;
                vertical_direction -= ev.motion.yrel * VELOCITY_LOOK;
                
                // Limitar pitch
                const float MAX_PITCH = 1.5f;
                if (vertical_direction > MAX_PITCH) vertical_direction = MAX_PITCH;
                if (vertical_direction < -MAX_PITCH) vertical_direction = -MAX_PITCH;

                float yaw = horizontal_direction;
                float pitch = vertical_direction;

                offsetCenter.x = cos(pitch) * sin(yaw);
                offsetCenter.y = sin(pitch);
                offsetCenter.z = -cos(pitch) * cos(yaw);
                
                break;
            }
            case SDL_KEYDOWN:{
                if(ev.key.keysym.sym == SDLK_ESCAPE) {
                    // Toggle mouse capture
                    SDL_bool mode = SDL_GetRelativeMouseMode();
                    SDL_SetRelativeMouseMode(mode == SDL_TRUE ? SDL_FALSE : SDL_TRUE);
                }
                if(ev.key.keysym.sym == SDLK_w) mov_front = true;
                if(ev.key.keysym.sym == SDLK_s) mov_back = true;
                if(ev.key.keysym.sym == SDLK_d) mov_right = true;
                if(ev.key.keysym.sym == SDLK_a) mov_left = true;
                if(ev.key.keysym.sym == SDLK_SPACE) mov_up = true;
                if(ev.key.keysym.sym == SDLK_LSHIFT) mov_down = true;
                break;
            }
            case SDL_KEYUP: {
                if(ev.key.keysym.sym == SDLK_w) mov_front = false;
                if(ev.key.keysym.sym == SDLK_s) mov_back = false;
                if(ev.key.keysym.sym == SDLK_d) mov_right = false;
                if(ev.key.keysym.sym == SDLK_a) mov_left = false;
                if(ev.key.keysym.sym == SDLK_SPACE) mov_up = false;
                if(ev.key.keysym.sym == SDLK_LSHIFT) mov_down = false;
                break;
            }
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int w = ev.window.data1;
                    int h = ev.window.data2;
                    view->setViewport({0, 0, (uint32_t)w, (uint32_t)h});
                    camera->setProjection(45.0f, float(w) / float(h), 0.1f, 2000.0f);
                }
                break;
            }
        }

        // Calcular vetores de movimento
        filament::math::vec3<float> front = normalize(offsetCenter);
        filament::math::vec3<float> upAbsolute(0, 1, 0);
        filament::math::vec3<float> right = normalize(cross(front, upAbsolute));
        filament::math::vec3<float> up = cross(right, front);
        
        filament::math::vec3<float> movement(0, 0, 0);
        if( mov_front ) movement += front;
        if( mov_back ) movement -= front;
        if( mov_right ) movement += right;
        if( mov_left ) movement -= right;
        if( mov_up ) movement += upAbsolute; // Usar up absoluto para subir/descer
        if( mov_down ) movement -= upAbsolute;

        if(length(movement) > 0.001f)
        {
            offsetEye += normalize(movement) * deltaTime * VELOCITY_MOVEMENT;
        }

        camera->lookAt(offsetEye, offsetEye + offsetCenter, upAbsolute);

        if (renderer->beginFrame(swapChain)) {
            renderer->render(view);
            renderer->endFrame();
            
            // if (frameCount < 5) {
            //     std::cout << "Frame " << frameCount << " rendered successfully" << std::endl;
            // }
            frameCount++;
        } else {
            std::cerr << "beginFrame failed!" << std::endl;
        }

        // Dar tempo para o sistema processar
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "Shutting down..." << std::endl;
    
    // Cleanup
    engine->destroy(lightEntity);
    engine->destroy(renderer);
    engine->destroy(swapChain);
    engine->destroy(view);
    engine->destroy(scene);
    engine->destroyCameraComponent(cameraEntity);
    em.destroy(cameraEntity);
    
    if (asset) {
        asset->releaseSourceData();
    }
    
    filament::Engine::destroy(&engine);
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    std::cout << "Cleanup complete" << std::endl;
    
    return 0;
}