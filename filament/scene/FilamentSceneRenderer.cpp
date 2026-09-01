#include <filament/scene/FilamentSceneRenderer.h>

#include <filament/Engine.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <filament/LightManager.h>
#include <utils/EntityManager.h>

#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>
#include <filament/utils/FilamentUtils.h>
#include <filament/view/SDL/SDLFilamentView.h>
#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>

#include <iostream>

namespace lite {

FilamentSceneRenderer::FilamentSceneRenderer(View* view, int width, int height)
    : SceneRenderer<FilamentScene>(view, width, height)
{
    // THREADING: última instrução do construtor — o objeto já está completo,
    // então a render thread pode despachar as fases virtuais com segurança.
    launchRenderThread();
}

FilamentSceneRenderer::~FilamentSceneRenderer() {
    // THREADING: join aqui, antes da parte derivada do objeto ser destruída
    // (a render thread executa overrides desta classe até o fim do cleanup()).
    stop();
}

void FilamentSceneRenderer::setIBL(const std::string& path, float intensity) {
    postCommand([this, path, intensity]() {
        m_ibl = std::make_unique<FilamentIBL>(m_engine, m_scene->getFilamentScene());
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

bool FilamentSceneRenderer::setup() {
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
        return false;
    }

    FilamentUtils::setEngine(m_engine);

    SDLFilamentView* currentView = dynamic_cast<SDLFilamentView*>(m_view);
    
    if(!currentView)
    {
        std::cerr << "FilamentSceneRenderer: View is not compatible" << std::endl;
        return false;  // cleanup() destrói o engine
    }

    // --- SwapChain ---
    m_swapChain = m_engine->createSwapChain(currentView->getNativeWindow());
    if (!m_swapChain) {
        std::cerr << "FilamentSceneRenderer: failed to create swap chain" << std::endl;
        return false;  // cleanup() destrói o engine
    }

    // --- Scene + factory + UI ---
    filament::Scene* fScene = m_engine->createScene();
    m_scene = std::make_unique<FilamentScene>(
        std::make_unique<FilamentInstanceFactory>(m_engine, fScene),
        std::make_unique<CEF_Filament_UIRendererThreaded>(m_engine, m_width, m_height),
        m_engine->createRenderer(),
        fScene,
        m_engine->createView(),
        m_swapChain
    );

    // --- UI: recursos GPU criados aqui, na thread do Engine (thread affinity) ---
    // Antes do set_value(): garante que existem quando waitReady() retornar na main.
    m_scene->getCurrentUI()->createFilamentResources();

    // --- Camera ---
    m_camera = std::make_unique<FilamentCameraAsset3dInstance>(m_engine);

    // --- View setup ---
    auto* view = m_scene->getFilamentView();
    view->setCamera(m_camera->getFilamentCamera());
    view->setScene(m_scene->getFilamentScene());
    view->setPostProcessingEnabled(true);
    view->setViewport({0, 0, (uint32_t)m_width, (uint32_t)m_height});
    m_camera->setProjection(45.0f, float(m_width) / float(m_height), 0.1f, 2000.0f);

    return true;
}

void FilamentSceneRenderer::renderFrame(float dt) {
    glm::vec3 eye, target;
    if (takePendingCamera(eye, target)) {
        m_camera->getTransform()->setPosition(eye);
        m_camera->lookAt(target);
    }

    m_scene->update(dt);
}

void FilamentSceneRenderer::cleanup() {
    // Roda na render thread (thread affinity). Tolerante a setup parcial:
    // falha de engine/swapchain passa pelo mesmo caminho do shutdown normal.
    m_ibl.reset();

    if (m_scene) {
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
        m_engine->destroy(filamentView);
        m_engine->destroy(filamentScene);
    }

    if (m_engine) {
        if (m_swapChain) {
            m_engine->destroy(m_swapChain);
            m_swapChain = nullptr;
        }

        FilamentUtils::setEngine(nullptr);
        filament::Engine::destroy(&m_engine);  // também zera m_engine
    }
}

} // namespace lite
