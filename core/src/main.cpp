// NOMINMAX deve vir antes de qualquer include Windows/CEF
#define NOMINMAX

#include <memory.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <functional>

#include <SDL.h>
#include <SDL_syswm.h>
#define SDL_MAIN_HANDLED

#include <assimp/importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>

#include <filament/Engine.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Scene.h>
#include <filament/Viewport.h>
#include <filament/Renderer.h>
#include <filament/IndexBuffer.h>
#include <camutils/Manipulator.h>
#include <utils/EntityManager.h>
#include <filament/TransformManager.h>
#include <filament/RenderableManager.h>
#include <utils/NameComponentManager.h>
#include <filament/LightManager.h>
#include <filament/lightning/FilamentIBL.h>
#include <filament/IndirectLight.h>
// Camera abstraction (replaces direct filament::Camera usage)
#include <filament/data/assets/FilamentCameraAsset3dInstance.h>
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

// Include bluegl para OpenGL loader do Filament
#include <bluegl/BlueGL.h>

// Old loader (to be deprecated)
// #include <lite/services/Asset3dLoader.h>
// #include <lite/services/Asset3dLoaderAssimp.h>
// #include <lite/services/MaterialLoader.h>

// New agnostic architecture
#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MaterialData.h>
#include <core/data/assets/Asset3dInstance.h>
#include <core/data/assets/MeshAsset3dInstance.h>
#include <assimp/assets/importer/AssimpImporter.h>
#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>
#include <filament/editor/FilamentWireframeSystem.h>
#include <core/ui/UIRendererThreaded.h>
#include "include/cef_app.h"

// GLM for renderer-agnostic math
#include <glm/glm.hpp>

using namespace std;
using namespace lite;

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

/*---------------------------------------------------------------------------------------
ASSETS
---------------------------------------------------------------------------------------*/

static std::ifstream::pos_type getFileSize(const char* filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
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

int main(int argc, char** argv){

    /*----------------------------------------------------------------------------
    CEF SUBPROCESS HANDLING - DEVE SER O PRIMEIRO!
    O CEF ainda cria subprocessos mesmo com thread separada.
    ----------------------------------------------------------------------------*/
    CefMainArgs cef_args(GetModuleHandle(nullptr));
    int exit_code = CefExecuteProcess(cef_args, nullptr, nullptr);
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

    // Logo após SDL_Init e antes de criar engine
    #ifdef _WIN32
    // Forçar carregamento de opengl32.dll do sistema (não mesa/software)
    SetEnvironmentVariableA("OPENGL_DRIVER", "opengl32");
    #endif

    // Não criar contexto OpenGL com SDL - deixar Filament fazer isso
    // Apenas criar a janela normal
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
    
    std::cout << "Window created successfully" << std::endl;

    /*----------------------------------------------------------------------------
    SETUP RENDERER ENGINE
    ----------------------------------------------------------------------------*/
    void* nativeWindow = getNativeWindowHandle(window);
    if (!nativeWindow) {
        std::cerr << "Failed to get native window handle" << std::endl;
        return -1;
    }
    
    std::cout << "Creating Filament engine..." << std::endl;
    
    // Tentar criar engine com Vulkan primeiro (melhor detecção de GPU)
    filament::Engine* engine = filament::Engine::Builder()
        .backend(filament::Engine::Backend::VULKAN)
        .build();

    // Se Vulkan falhar, fallback para OpenGL
    if (!engine) {
        std::cout << "Vulkan not available, falling back to OpenGL..." << std::endl;
        engine = filament::Engine::Builder()
            .backend(filament::Engine::Backend::OPENGL)
            .build();
    }

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
    auto camera = std::make_unique<FilamentCameraAsset3dInstance>(engine);

    filament::Scene* scene = engine->createScene();
    filament::View* view = engine->createView();

    // Configurar view
    view->setCamera(camera->getFilamentCamera());
    view->setScene(scene);
    view->setPostProcessingEnabled(true);

    int fbW = SCREEN_WIDTH, fbH = SCREEN_HEIGHT;
    SDL_GetWindowSize(window, &fbW, &fbH);
    view->setViewport({0, 0, (uint32_t)fbW, (uint32_t)fbH});

    // Configurar câmera DEPOIS de criar view
    camera->setProjection(45.0f, float(fbW) / float(fbH), 0.1f, 2000.0f);
    // camera->setViewport(fbW, fbH);
    
    std::cout << "Scene and View created successfully" << std::endl;

    /*----------------------------------------------------------------------------
    LOAD SCENE ENTITIES
    ----------------------------------------------------------------------------*/
    // std::cout << "Loading GLTF asset..." << std::endl;
    
    // filament::gltfio::FilamentAsset* asset = nullptr;
    // filament::gltfio::FilamentInstance* gltfInstance = loadAsset(
    //     engine, 
    //     scene, 
    //     asset, 
    //     "C:/Users/pixqu/Downloads/coast_african_rock_m_17_wghiado_raw.glb"
    // );
    
    // if(asset && gltfInstance)
    // {
    //     std::cout << "Loading resources..." << std::endl;
    //     loadResources(
    //         engine, 
    //         asset, 
    //         gltfInstance, 
    //         "C:/Users/pixqu/Downloads/coast_african_rock_m_17_wghiado_raw.glb"
    //     );
    //     std::cout << "Resources loaded successfully" << std::endl;
    // } else {
    //     std::cerr << "Failed to load GLTF asset!" << std::endl;
    // }

    // New agnostic architecture: Importer + InstanceFactory
    auto importer = std::make_unique<AssimpImporter>();
    auto instanceFactory = std::make_unique<FilamentInstanceFactory>(engine, scene);

    // Import 3D asset (renderer-agnostic data)
    Asset3dData rootNode;
    std::vector<MaterialData> materials;

    std::unique_ptr<Asset3dInstance> assetInstance;
    if (importer->import("C:/Users/pixqu/Downloads/Jason Stalhart/Base_Mesh/Aiden_Stallhart_BaseMesh_skeleton_Ver1.fbx", rootNode, materials)) {
        // Instantiate to GPU (Filament-specific)
        assetInstance = instanceFactory->instantiate(rootNode, materials);
    }


    glm::vec3 center(0, 0, 0);
    float radius = 5.0f;
    // // Calcular bounding box do asset para posicionar câmera

    // if (asset) {
    //     auto bbox = asset->getBoundingBox();
    //     center = bbox.center();
    //     radius = length(bbox.extent()) * 1.5f;
    //     std::cout << "Asset center: " << center.x << ", " << center.y << ", " << center.z << std::endl;
    //     std::cout << "Asset radius: " << radius << std::endl;
    // }

    //SETUP CAMERA - posicionar olhando para o modelo
    glm::vec3 offsetEye = center + glm::vec3(0, radius * 0.3f, radius);
    glm::vec3 offsetCenter = glm::normalize(center - offsetEye);

    // Initialize camera position via transform
    camera->getTransform()->setPosition(offsetEye);
    
    //SETUP LIGHTNING IBL
    std::string iblPath = "D:/Workspace/LiteEngine/3rd_party/filament/out/samples/assets/ibl/lightroom_14b";

    auto ibl = std::make_unique<FilamentIBL>(engine, scene);
    if (!ibl->load(iblPath)) {
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
        .castShadows(false)
        .build(*engine, lightEntity);
    scene->addEntity(lightEntity);
    
    std::cout << "Lighting setup complete" << std::endl;

    /*----------------------------------------------------------------------------
    SETUP WIREFRAME SYSTEM
    ----------------------------------------------------------------------------*/
    auto wireframeSystem = std::make_unique<FilamentWireframeSystem>(engine, scene);
    wireframeSystem->initialize(fbW, fbH);
    wireframeSystem->loadMaterial("D:/Workspace/LiteEngine/core/resources/filament/editor/materials/wireframe.filamat");
    wireframeSystem->setWireframeColor(glm::vec4(1.0f, 1.0f, 1.0f, 0.6f)); // White with some transparency

    // Add all meshes from assetInstance to wireframe
    if (assetInstance) {
        std::function<void(Asset3dInstance*)> addMeshesToWireframe = [&](Asset3dInstance* node) {
            if (node->isMesh()) {
                wireframeSystem->addWireframeMesh(dynamic_cast<MeshAsset3dInstance*>(node));
            }
            for (auto& child : node->children) {
                addMeshesToWireframe(child.get());
            }
        };
        addMeshesToWireframe(assetInstance.get());
    }

    /*----------------------------------------------------------------------------
    SETUP UI RENDERER (CEF) - Thread separada
    ----------------------------------------------------------------------------*/
    lite::UIRendererThreaded* uiRenderer = new lite::UIRendererThreaded(engine, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!uiRenderer->start("file:///D:/Workspace/LiteEngine/core/resources/UI/index.html")) {
        std::cerr << "Failed to start UIRenderer" << std::endl;
    }

    std::cout << "Starting main loop..." << std::endl;

    /*----------------------------------------------------------------------------
    MAIN LOOP
    ----------------------------------------------------------------------------*/
    
    bool running = true;
    Uint64 prevTicks = SDL_GetPerformanceCounter();
    
    //NAVIGATION
    bool mov_front = false, mov_back = false, mov_right = false, mov_left = false, mov_up = false, mov_down = false;
    const float VELOCITY_MOVEMENT = radius * 50.f;
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
                    // camera->setViewport(w, h);
                }
                break;
            }
        }

        // Calcular vetores de movimento
        glm::vec3 front = glm::normalize(offsetCenter);
        glm::vec3 upAbsolute(0, 1, 0);
        glm::vec3 right = glm::normalize(glm::cross(front, upAbsolute));
        glm::vec3 up = glm::cross(right, front);

        glm::vec3 movement(0, 0, 0);
        if( mov_front ) movement += front;
        if( mov_back ) movement -= front;
        if( mov_right ) movement += right;
        if( mov_left ) movement -= right;
        if( mov_up ) movement += upAbsolute;
        if( mov_down ) movement -= upAbsolute;

        if(glm::length(movement) > 0.001f)
        {
            offsetEye += glm::normalize(movement) * deltaTime * VELOCITY_MOVEMENT;
        }

        // Update camera position via transform, then lookAt uses it
        camera->getTransform()->setPosition(offsetEye);
        camera->lookAt(offsetEye + offsetCenter);

        // Update wireframe transforms
        wireframeSystem->beginFrame();

        if (renderer->beginFrame(swapChain)) {
            renderer->render(view);

            // UI Overlay (CEF) - apenas atualiza textura, CEF roda em outra thread
            if (uiRenderer) {
                uiRenderer->updateTexture();

                // Enviar posicao do mouse para o JavaScript
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                uiRenderer->executeJavaScript(
                    "updateMousePosition(" + std::to_string(mouseX) + "," + std::to_string(mouseY) + ");"
                );

                uiRenderer->render(renderer);
            }

            renderer->endFrame();
            frameCount++;
        } else {
            std::cerr << "beginFrame failed!" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "Shutting down..." << std::endl;

    // Cleanup UI - para a thread do CEF primeiro
    if (uiRenderer) {
        uiRenderer->stop();
        delete uiRenderer;
        uiRenderer = nullptr;
    }

    // Cleanup wireframe system
    wireframeSystem.reset();

    // Cleanup
    engine->destroy(lightEntity);
    engine->destroy(renderer);
    engine->destroy(swapChain);
    engine->destroy(view);
    engine->destroy(scene);
    camera.reset(); // unique_ptr cleanup (destructor handles Filament cleanup)
    
    // if (asset) {
    //     asset->releaseSourceData();
    // }

    // Cleanup 3D asset instance using the new architecture
    if (assetInstance) {
        instanceFactory->destroy(assetInstance.get());
        assetInstance.reset();
    }
    instanceFactory.reset();
    importer.reset();
    
    filament::Engine::destroy(&engine);
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    std::cout << "Cleanup complete" << std::endl;
    
    return 0;
}