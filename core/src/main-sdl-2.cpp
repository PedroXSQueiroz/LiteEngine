#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <SDL_syswm.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <chrono>
#include <set>

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
#include <filament/Material.h>
#include <filament/RenderableManager.h>
#include <filamentapp/IBL.h>
#include <camutils/Manipulator.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <uberarchive.h>

#include <utils/EntityManager.h>
#include <utils/NameComponentManager.h>
#include <utils/Path.h>
#include <utils/Panic.h>

using namespace filament;
using namespace filament::gltfio;
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

// lê todo o arquivo em um vector<uint8_t>
static std::vector<uint8_t> readFileToBuffer(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("can't open " + path);
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(size);
    if (!in.read(reinterpret_cast<char*>(buf.data()), size))
        throw std::runtime_error("read error " + path);
    return buf;
}

// bool loadGlbIntoScene(Engine* engine, Scene* scene, const std::string& filepath) {
//     try {
//         auto buffer = readFileToBuffer(filepath); // .glb binário

//         MaterialProvider* matProv = createUbershaderProvider(engine, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);

//         // 1) criar AssetLoader — (construtor/factory depende da versão; a maioria possui um create())
//         // AssetConfiguration config = { 
//         //         engine,
//         //         matProv,
//         //         new NameComponentManager(EntityManager::get())
//         // };
//         AssetLoader* loader = AssetLoader::create({ 
//                 engine,
//                 matProv,
//                 new NameComponentManager(EntityManager::get())
//         });
// #if 1
//         // Tente a factory create() se existir (algumas versões usam create())
//         // Se o seu header não tiver create(), substitua por AssetLoader(...) conforme o seu include.
        
// #else
//         // Alternativa (dependendo da sua build do gltfio):
//         // loader = std::make_unique<AssetLoader>(engine);
// #endif

//         // 2) criar asset a partir do buffer
//         FilamentAsset* asset = nullptr;
//         // → Versão A (muito comum): createAssetFromBinary
//         if constexpr(true) {
//             // NOTE: substitua por createAsset() se for essa API na sua versão.
//         }
//         asset = loader->createAsset(buffer.data(), static_cast<uint32_t>(buffer.size()));

//         // if (!asset) {
//         //     std::cerr << "failed to create FilamentAsset from " << filepath << std::endl;
//         //     loader->destroy(); // se aplicável
//         //     return false;
//         // }

//         // 3) ResourceLoader — carrega texturas e buffers para GPU
//         ResourceLoader resourceLoader({engine});
//         // resourceLoader.addResourcePath("C:/Users/pixqu/Downloads/gltf_test_pbr_material/");
//         resourceLoader.loadResources(asset); // bloco-síncrono; existe também versão assíncrona

//         // 4) adicionar entidades ao scene
//         const auto* entities = asset->getEntities();
//         size_t count = asset->getEntityCount();
//         if (count > 0) {
//             scene->addEntities(entities, static_cast<uint32_t>(count));
//         }

//         // NOTE: mantenha 'loader', 'resourceLoader' e 'asset' vivos enquanto usar as entidades
//         // normalmente você guarda FilamentAsset* em algum container até destruir com loader->destroyAsset(asset).

//         return true;
//     } catch (const std::exception& e) {
//         std::cerr << "exception loading glb: " << e.what() << std::endl;
//         return false;
//     }
// }

// TextureProvider* createStbProvider(Engine* engine) {
//     return new StbProvider(engine);
// }

// TextureProvider* createKtx2Provider(Engine* engine) {
//     return new Ktx2Provider(engine);
// }


auto loadResources = [&] (Engine* engine, FilamentAsset* asset, FilamentInstance* instance, const utils::Path& filename) {
        // Load external textures and buffers.
        std::string const gltfPath = filename.getAbsolutePath();
        ResourceConfiguration configuration = {};
        configuration.engine = engine;
        configuration.gltfPath = gltfPath.c_str();
        configuration.normalizeSkinningWeights = true;

        ResourceLoader* resourceLoader = new gltfio::ResourceLoader(configuration);
        // app.stbDecoder = createStbProvider(engine);
        // app.ktxDecoder = createKtx2Provider(engine);
        resourceLoader->addTextureProvider("image/png", createStbProvider(engine));
        resourceLoader->addTextureProvider("image/jpeg", createStbProvider(engine));
        resourceLoader->addTextureProvider("image/ktx2", createKtx2Provider(engine));
        // if (!resourceLoader) {
        // } else {
        //     app.resourceLoader->setConfiguration(configuration);
        // }

        // if (!resourceLoader->asyncBeginLoad(asset)) {
        //     std::cerr << "Unable to start loading resources for " << filename << std::endl;
        //     exit(1);
        // }

        resourceLoader->loadResources(asset);
        resourceLoader->asyncUpdateLoad();

        // if (app.recomputeAabb) {
        //     app.asset->getInstance()->recomputeBoundingBoxes();
        // }

        asset->releaseSourceData();

        // Enable stencil writes on all material instances.
        const size_t matInstanceCount = instance->getMaterialInstanceCount();
        MaterialInstance* const* const instances = instance->getMaterialInstances();
        for (int mi = 0; mi < matInstanceCount; mi++) {
            instances[mi]->setStencilWrite(true);
            instances[mi]->setStencilOpDepthStencilPass(MaterialInstance::StencilOperation::INCR);
        }
        // setupIBL();
    };

static std::ifstream::pos_type getFileSize(const char* filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

FilamentInstance* loadAsset(Engine* engine, Scene* scene, FilamentAsset* &assetOut, const utils::Path& filename) {
        // Peek at the file size to allow pre-allocation.
        long const contentSize = static_cast<long>(getFileSize(filename.c_str()));
        if (contentSize <= 0) {
            std::cerr << "Unable to open " << filename << std::endl;
            exit(1);
        }

        // Consume the glTF file.
        std::ifstream in(filename.c_str(), std::ifstream::binary | std::ifstream::in);
        std::vector<uint8_t> buffer(static_cast<unsigned long>(contentSize));
        if (!in.read((char*) buffer.data(), contentSize)) {
            std::cerr << "Unable to read " << filename << std::endl;
            exit(1);
        }

        MaterialProvider* matProv = createUbershaderProvider(engine, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);
        AssetLoader* assetLoader = AssetLoader::create({ 
            engine,
            matProv,
            new NameComponentManager(EntityManager::get())
        });

        // Parse the glTF file and create Filament entities.
        FilamentAsset* asset = assetLoader->createAsset(buffer.data(), buffer.size());
        if (!asset) {
            std::cerr << "Unable to parse " << filename << std::endl;
            exit(1);
        }

        // pre-compile all material variants
        std::set<Material*> materials;
        RenderableManager const& rcm = engine->getRenderableManager();
        Slice<Entity> const renderables{
                asset->getRenderableEntities(), asset->getRenderableEntityCount() };
        for (Entity const e: renderables) {
            auto ri = rcm.getInstance(e);
            size_t const c = rcm.getPrimitiveCount(ri);
            for (size_t i = 0; i < c; i++) {
                MaterialInstance* const mi = rcm.getMaterialInstanceAt(ri, i);
                Material* ma = const_cast<Material *>(mi->getMaterial());
                materials.insert(ma);
            }
        }
        for (Material* ma : materials) {
            // Don't attempt to precompile shaders on WebGL.
            // Chrome already suffers from slow shader compilation:
            // https://github.com/google/filament/issues/6615
            // Precompiling shaders exacerbates the problem.
#if !defined(__EMSCRIPTEN__)
            // First compile high priority variants
            ma->compile(Material::CompilerPriorityQueue::HIGH,
                    UserVariantFilterBit::DIRECTIONAL_LIGHTING |
                    UserVariantFilterBit::DYNAMIC_LIGHTING |
                    UserVariantFilterBit::SHADOW_RECEIVER);

            // and then, everything else at low priority, except STE, which is very uncommon.
            ma->compile(Material::CompilerPriorityQueue::LOW,
                    UserVariantFilterBit::FOG |
                    UserVariantFilterBit::SKINNING |
                    UserVariantFilterBit::SSR |
                    UserVariantFilterBit::VSM);
#endif
        }

        FilamentInstance* instance = asset->getInstance();
        scene->addEntities(asset->getEntities(), asset->getEntityCount());
        buffer.clear();
        buffer.shrink_to_fit();

        assetOut = asset;

        return instance;
    };

int main(int /*argc*/, char** /*argv*/) {
    const int SCREEN_WIDTH = 1980;
    const int SCREEN_HEIGHT = 1080;

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
    camera->lookAt({0, 0, 0}, {0, 0, 0}, {0, 1, 0});

    filament::camutils::Manipulator<float>* cameraMan = filament::camutils::Manipulator<float>::Builder()
        .targetPosition(0, 0, 0)
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
    
    std::unique_ptr<IBL> ibl = loadIBLUnique(engine, iblPath, scene);
    if (!ibl) {
        std::cerr << "Warning: IBL failed to load. Scene may be dark." << std::endl;
    } else {
        std::cout << "IBL loaded and applied to scene." << std::endl;
    }

    // if( loadGlbIntoScene(engine, scene, "C:/Users/pixqu/Downloads/gltf_test_pbr_material/scene.gltf") ) 
    // {
    //     std::cout << "scene loaded" << std::endl;
    // }
    // else
    // {
    //     std::cerr << "error on load scene" << std::endl;
    // }

    FilamentAsset* asset;
    FilamentInstance* gltfInstance = loadAsset(engine, scene, asset, "C:/Users/pixqu/Downloads/gltf_test_pbr_material/scene.gltf");
    if(asset && gltfInstance)
    {
        loadResources(engine, asset, gltfInstance, "C:/Users/pixqu/Downloads/gltf_test_pbr_material/scene.gltf");
    }

    // Loop principal
    bool running = true;
    
    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    cameraMan->grabBegin(mouseX, mouseY, false);

    while (running) {
        SDL_Event ev;
        filament::math::float3 eye, center, up;
        cameraMan->getLookAt(&eye, &center, &up);
        while (SDL_PollEvent(&ev)) {
            
            
            switch (ev.type)
            {
            case SDL_QUIT:
                running = false;
                /* code */
                break;

            case SDL_MOUSEMOTION:{
                cameraMan->grabUpdate(ev.motion.x, ev.motion.y);
                
                //FIXME?: IS HAPPENING TWICE
                cameraMan->getLookAt(&eye, &center, &up);
                
                break;
            }
            case SDL_KEYDOWN:{
                
                if(ev.key.keysym.sym == SDLK_w)
                {
                    std::cout << "pressed W" << std::endl;
                    filament::math::float3 moveDirection(eye.x, eye.y, eye.z);
                    filament::math::float3 movement = moveDirection * 0.001;
                    center += movement;
                }
            }
        }
        
        // if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_RESIZED) {
            //     int w = ev.window.data1;
            //     int h = ev.window.data2;
            //     view->setViewport({0, 0, (uint32_t)w, (uint32_t)h});
            // }
            
        }

        camera->lookAt(eye, center, up);

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
