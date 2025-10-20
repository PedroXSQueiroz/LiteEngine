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
#include <filameshio/MeshReader.h>
#include <filamentapp/IBL.h>
#include <camutils/Manipulator.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <uberarchive.h>
#include <math/mat4.h>
#include <math/vec3.h>

#include <utils/EntityManager.h>
#include <utils/NameComponentManager.h>
#include <utils/Path.h>
#include <utils/Panic.h>

using namespace filament;
using namespace filament::gltfio;
using namespace filamesh;
using namespace camutils;
using namespace utils;
using namespace filament::math;
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

bool loadFilamesh(Engine* engine, Scene* scene, const std::string& path) {
    // 1. Ler o arquivo filamesh
    MeshReader::MaterialRegistry materials;
    
    MeshReader::Mesh mesh = MeshReader::loadMeshFromFile(engine, Path(path.c_str()), materials);
    
    // 2. Criar MaterialInstance
    // MaterialInstance* materialInstance = material->createInstance();

    // 3. Criar entidade renderizável
    // Entity renderable = EntityManager::get().create();

    // // utils::CString* materialsNames = nullptr;

    // // materials.getRegisteredMaterialNames(materialsNames);

    // RenderableManager::Builder(1)
    //     // .boundingBox(mesh.boundingBox)
    //     .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, mesh.vertexBuffer, mesh.indexBuffer)
    //     // .material(0, materials.getMaterialInstance(*materialsNames))
    //     .culling(true)
    //     .build(*engine, renderable);

    // 4. Adicionar à cena
    scene->addEntity(mesh.renderable);

    return true;
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


void loadResources(Engine* engine, FilamentAsset* asset, FilamentInstance* instance, const utils::Path& filename) {
        // Load external textures and buffers.
        std::string const gltfPath = filename.getAbsolutePath();
        ResourceConfiguration configuration = {};
        configuration.engine = engine;
        configuration.gltfPath = gltfPath.c_str();
        configuration.normalizeSkinningWeights = true;

        ResourceLoader* resourceLoader = new gltfio::ResourceLoader(configuration);
        resourceLoader->addTextureProvider("image/png", createStbProvider(engine));
        resourceLoader->addTextureProvider("image/jpeg", createStbProvider(engine));
        resourceLoader->addTextureProvider("image/ktx2", createKtx2Provider(engine));
        
        resourceLoader->loadResources(asset);
        resourceLoader->asyncUpdateLoad();

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
        .build(filament::camutils::Mode::FREE_FLIGHT);

    view->setCamera(camera);
    view->setScene(scene);
    view->setViewport({0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
    cameraMan->setViewport(SCREEN_WIDTH, SCREEN_HEIGHT);

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

    FilamentAsset* asset;
    FilamentInstance* gltfInstance = loadAsset(engine, scene, asset, "C:/Users/pixqu/Downloads/uploads_files_2395268_Yacht.glb");
    if(asset && gltfInstance)
    {
        loadResources(engine, asset, gltfInstance, "C:/Users/pixqu/Downloads/uploads_files_2395268_Yacht.glb");
    }

    // loadFilamesh(engine, scene, "C:/Users/pixqu/Downloads/Bistro_v5_2/Bistro_v5_2/Yacht.filamesh");

    // Loop principal
    bool running = true;
    
    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    cameraMan->grabBegin(mouseX, mouseY, false);

    filament::math::float3 eye, center, up;
    filament::math::float3 offsetEye, offsetCenter;
    
    bool mov_front = false, mov_back = false, mov_right = false, mov_left = false, mov_up = false, mov_down = false; 
    
    Uint64 prevTicks = SDL_GetPerformanceCounter();

    const float VELOCITY_MOVEMENT = 7;
    const float VELOCITY_LOOK = 5;
    float horizontal_direction = 0, vertical_direction = 0;


    while (running) {
        
        Uint64 now = SDL_GetPerformanceCounter();
        float deltaTime = (float)((now - prevTicks) / (double)SDL_GetPerformanceFrequency());
        prevTicks = now;
        
        SDL_Event ev;
        cameraMan->getLookAt(&eye, &center, &up);


        while (SDL_PollEvent(&ev)) {
            
            
            switch (ev.type)
            {
            case SDL_QUIT:
                running = false;
                /* code */
                break;

            case SDL_MOUSEMOTION:{
                
                
                // cameraMan->grabUpdate(ev.motion.x, ev.motion.y);

                std::cout << std::printf("mouse motion: x: %i y: %i", ev.motion.xrel, ev.motion.yrel) << std::endl;
                
                horizontal_direction += ev.motion.xrel * VELOCITY_LOOK * deltaTime;
                vertical_direction += ev.motion.yrel * VELOCITY_LOOK * deltaTime;

                float yaw   = horizontal_direction;
                float pitch = vertical_direction;

                offsetCenter.x = cos(pitch) * cos(yaw);
                offsetCenter.y = sin(pitch);
                offsetCenter.z = cos(pitch) * sin(yaw);

                float3 dummyCenter(0, 0, 0);
                //FIXME?: IS HAPPENING TWICE
                cameraMan->getLookAt(&eye, &dummyCenter, &up);
                
                break;
            }
            case SDL_KEYDOWN:{
                

                if(ev.key.keysym.sym == SDLK_w) { mov_front = true; std::cout << "pressed w key" << std::endl; }
                if(ev.key.keysym.sym == SDLK_s) { mov_back = true; std::cout << "pressed s key" << std::endl; }
                if(ev.key.keysym.sym == SDLK_d) { mov_right = true; std::cout << "pressed d key" << std::endl; }
                if(ev.key.keysym.sym == SDLK_a) { mov_left = true; std::cout << "pressed a key" << std::endl; }
                if(ev.key.keysym.sym == SDLK_q) { mov_up = true; std::cout << "pressed q key" << std::endl; }
                if(ev.key.keysym.sym == SDLK_e) { mov_down = true; std::cout << "pressed e key" << std::endl; }

                }
            break;
            case SDL_KEYUP: {

                    if(ev.key.keysym.sym == SDLK_w) {mov_front = false; std::cout << "released w key" << std::endl; }
                    if(ev.key.keysym.sym == SDLK_s) {mov_back = false; std::cout << "released s key" << std::endl; }
                    if(ev.key.keysym.sym == SDLK_d) {mov_right = false; std::cout << "released d key" << std::endl; }
                    if(ev.key.keysym.sym == SDLK_a) {mov_left = false; std::cout << "released a key" << std::endl; }
                    if(ev.key.keysym.sym == SDLK_q) {mov_up = false; std::cout << "released q key" << std::endl; }
                    if(ev.key.keysym.sym == SDLK_e) {mov_down = false; std::cout << "released e key" << std::endl; }

                }
            break;
            }
        
        // if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_RESIZED) {
            //     int w = ev.window.data1;
            //     int h = ev.window.data2;
            //     view->setViewport({0, 0, (uint32_t)w, (uint32_t)h});
            // }
            
        }

        // cameraMan->update(deltaTime);

        filament::math::vec3<float> front = normalize(offsetCenter);
        filament::math::vec3<float> upAbsolute(0, 1, 0);
        filament::math::vec3<float> right = normalize(cross(front, upAbsolute));
        filament::math::vec3<float> up = cross(right, front);
        
        filament::math::vec3<float> movement;
        if( mov_front ) movement += front;
        if( mov_back ) movement -= front;
        if( mov_right ) movement += right;
        if( mov_left ) movement -= right;
        if( mov_up ) movement += up;
        if( mov_down ) movement -= up;

        if(     (mov_front || mov_back || mov_right || mov_left || mov_up || mov_down) 
            && !(mov_front && mov_back)
            && !(mov_right && mov_left)
            && !(mov_up && mov_down) 
        )
        {
            offsetEye += normalize( movement ) * deltaTime * VELOCITY_MOVEMENT;
        }

        camera->lookAt(offsetEye, offsetEye + offsetCenter, up);

        if (renderer->beginFrame(swapChain)) {
            renderer->render(view);
            renderer->endFrame();
        }

        // Em caso de renderização com Filament+SDL+GL we usually don't call SDL_GL_SwapWindow,
        // because Filament takes care of presenting. However Filament's OpenGL backend may
        // expect you to swap if SwapChain mapping requires it. If you see flicker, try toggling.
        // SDL_GL_SwapWindow(window);

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
