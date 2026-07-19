#include <filament/scene/FilamentSceneRenderer.h>

#include <filament/Engine.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <filament/LightManager.h>
#include <utils/EntityManager.h>

#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>
#include <filament/utils/FilamentUtils.h>
#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>

#include <chrono>
#include <iostream>

FilamentSceneRenderer::FilamentSceneRenderer(void* nativeWindowHandle, int width, int height)
    : m_nativeWindowHandle(nativeWindowHandle)
    , m_width(width)
    , m_height(height)
    , m_readyFuture(m_readyPromise.get_future())
{
    m_renderThread = std::thread(&FilamentSceneRenderer::renderLoop, this);
}

FilamentSceneRenderer::~FilamentSceneRenderer() {
    stop();
}

void FilamentSceneRenderer::waitReady() {
    m_readyFuture.wait();
}

void FilamentSceneRenderer::start() {
    m_started.store(true);
}

void FilamentSceneRenderer::stop() {
    m_running.store(false);
    m_started.store(true);  // unblock spin-wait if start() was never called
    if (m_renderThread.joinable()) {
        m_renderThread.join();
    }
}

FilamentScene* FilamentSceneRenderer::getScene() {
    return m_scene.get();
}

void FilamentSceneRenderer::setCameraState(const glm::vec3& eye, const glm::vec3& target) {
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    m_pendingCamera.eye = eye;
    m_pendingCamera.target = target;
    m_pendingCamera.dirty = true;
}

void FilamentSceneRenderer::setIBL(const std::string& path, float intensity) {
    postCommand([this, path, intensity]() {
        m_ibl = std::make_unique<lite::FilamentIBL>(m_engine, m_scene->getFilamentScene());
        if (!m_ibl->load(path)) {
            std::cerr << "Warning: IBL failed to load from " << path << std::endl;
            m_ibl.reset();
        } else {
            m_scene->getFilamentScene()->getIndirectLight()->setIntensity(intensity);
        }
    });
}

void FilamentSceneRenderer::addDirectionalLight(const glm::vec3& color, float intensity,
                                                const glm::vec3& direction, bool castShadows) {
    postCommand([this, color, intensity, direction, castShadows]() {
        m_lightEntity = utils::EntityManager::get().create();
        filament::LightManager::Builder(filament::LightManager::Type::SUN)
            .color({color.r, color.g, color.b})
            .intensity(intensity)
            .direction({direction.x, direction.y, direction.z})
            .castShadows(castShadows)
            .build(*m_engine, m_lightEntity);
        m_scene->getFilamentScene()->addEntity(m_lightEntity);
    });
}

void FilamentSceneRenderer::resize(int width, int height) {
    postCommand([this, width, height]() {
        m_width = width;
        m_height = height;
        m_scene->getFilamentView()->setViewport({0, 0, (uint32_t)width, (uint32_t)height});
        m_camera->setProjection(45.0f, float(width) / float(height), 0.1f, 2000.0f);
    });
}

void FilamentSceneRenderer::postCommand(std::function<void()> command) {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    m_commands.push(std::move(command));
}

void FilamentSceneRenderer::processCommands() {
    std::queue<std::function<void()>> batch;
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        std::swap(batch, m_commands);
    }
    while (!batch.empty()) {
        batch.front()();
        batch.pop();
    }
}

void FilamentSceneRenderer::renderLoop() {
    // --- Engine creation ---
    m_engine = filament::Engine::Builder()
        .backend(filament::Engine::Backend::VULKAN)
        .build();

    if (!m_engine) {
        std::cout << "Vulkan not available, falling back to OpenGL..." << std::endl;
        m_engine = filament::Engine::Builder()
            .backend(filament::Engine::Backend::OPENGL)
            .build();
    }

    if (!m_engine) {
        std::cerr << "FilamentSceneRenderer: failed to create engine" << std::endl;
        m_readyPromise.set_value();
        return;
    }

    FilamentUtils::setEngine(m_engine);

    // --- SwapChain ---
    m_swapChain = m_engine->createSwapChain(m_nativeWindowHandle);
    if (!m_swapChain) {
        std::cerr << "FilamentSceneRenderer: failed to create swap chain" << std::endl;
        filament::Engine::destroy(&m_engine);
        m_readyPromise.set_value();
        return;
    }

    // --- Scene + factory + UI ---
    filament::Scene* fScene = m_engine->createScene();
    m_scene = std::make_unique<FilamentScene>(
        std::make_unique<lite::FilamentInstanceFactory>(m_engine, fScene),
        std::make_unique<lite::CEF_Filament_UIRendererThreaded>(m_engine, m_width, m_height),
        m_engine->createRenderer(),
        fScene,
        m_engine->createView(),
        m_swapChain
    );

    // --- Camera ---
    m_camera = std::make_unique<lite::FilamentCameraAsset3dInstance>(m_engine);

    // --- View setup ---
    auto* view = m_scene->getFilamentView();
    view->setCamera(m_camera->getFilamentCamera());
    view->setScene(m_scene->getFilamentScene());
    view->setPostProcessingEnabled(true);
    view->setViewport({0, 0, (uint32_t)m_width, (uint32_t)m_height});
    m_camera->setProjection(45.0f, float(m_width) / float(m_height), 0.1f, 2000.0f);

    m_running.store(true);

    // Signal main thread: setup complete
    m_readyPromise.set_value();

    // Spin-wait for start() — process setup commands (IBL, lights, etc.) while waiting
    while (!m_started.load() && m_running.load()) {
        processCommands();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // --- UI: recursos GPU criados aqui, na thread do Engine (thread affinity) ---
    // Antes do set_value(): garante que existem quando waitReady() retornar na main.
    m_scene->getCurrentUI()->createFilamentResources();

    // Process any remaining setup commands before the first frame
    processCommands();

    // --- Render loop (B2: independent, no per-frame sync with main) ---
    auto prevTime = std::chrono::high_resolution_clock::now();

    while (m_running.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - prevTime).count();
        prevTime = now;

        processCommands();

        {
            std::lock_guard<std::mutex> lock(m_cameraMutex);
            if (m_pendingCamera.dirty) {
                m_camera->getTransform()->setPosition(m_pendingCamera.eye);
                m_camera->lookAt(m_pendingCamera.target);
                m_pendingCamera.dirty = false;
            }
        }

        m_scene->update(dt);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // --- Cleanup (on render thread, correct thread affinity) ---
    m_ibl.reset();

    auto* filamentRenderer = m_scene->getFilamentRenderer();
    auto* filamentView     = m_scene->getFilamentView();
    auto* filamentScene    = m_scene->getFilamentScene();
    auto* uiRenderer       = m_scene->getCurrentUI();

    if (uiRenderer) {
        uiRenderer->stop();
    }

    m_scene.reset();
    m_camera.reset();

    if (!m_lightEntity.isNull()) {
        m_engine->destroy(m_lightEntity);
    }

    m_engine->destroy(filamentRenderer);
    m_engine->destroy(m_swapChain);
    m_engine->destroy(filamentView);
    m_engine->destroy(filamentScene);

    filament::Engine::destroy(&m_engine);
}
