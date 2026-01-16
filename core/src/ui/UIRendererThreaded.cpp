// IMPORTANTE: Definir NOMINMAX antes de qualquer include para evitar conflito com CEF
#define NOMINMAX

#include <lite/ui/UIRendererThreaded.h>
#include <fstream>
#include <iostream>
#include <filament/TextureSampler.h>
#include <filament/Viewport.h>
#include <math/vec3.h>
#include <math/vec2.h>

namespace lite {

UIRendererThreaded::UIRendererThreaded(filament::Engine* engine, uint32_t width, uint32_t height)
    : m_engine(engine), m_width(width), m_height(height)
{
    m_pixelBuffer.resize(width * height * 4, 0);
}

UIRendererThreaded::~UIRendererThreaded() {
    stop();
}

bool UIRendererThreaded::start(const std::string& initialUrl) {
    std::cout << "[UIRendererThreaded] Starting..." << std::endl;

    // Criar recursos Filament na thread principal
    createFilamentResources();

    // Iniciar thread do CEF
    m_running = true;
    m_cefThread = std::thread(&UIRendererThreaded::cefThreadFunc, this, initialUrl);

    // Aguardar CEF ficar pronto (com timeout)
    int attempts = 0;
    while (!m_cefReady && m_running && attempts < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
    }

    if (!m_cefReady) {
        std::cerr << "[UIRendererThreaded] CEF failed to initialize" << std::endl;
        stop();
        return false;
    }

    std::cout << "[UIRendererThreaded] Ready" << std::endl;
    return true;
}

void UIRendererThreaded::stop() {
    if (!m_running) return;

    std::cout << "[UIRendererThreaded] Stopping..." << std::endl;
    m_running = false;

    if (m_cefThread.joinable()) {
        m_cefThread.join();
    }

    // Cleanup Filament (thread principal)
    if (m_engine) {
        if (!m_quadEntity.isNull()) m_engine->destroy(m_quadEntity);
        if (m_vertexBuffer) m_engine->destroy(m_vertexBuffer);
        if (m_indexBuffer) m_engine->destroy(m_indexBuffer);
        if (m_materialInstance) m_engine->destroy(m_materialInstance);
        if (m_material) m_engine->destroy(m_material);
        if (m_texture) m_engine->destroy(m_texture);
        if (m_view) m_engine->destroy(m_view);
        if (m_scene) m_engine->destroy(m_scene);
        if (m_camera) {
            m_engine->destroyCameraComponent(m_cameraEntity);
            utils::EntityManager::get().destroy(m_cameraEntity);
        }
    }

    std::cout << "[UIRendererThreaded] Stopped" << std::endl;
}

void UIRendererThreaded::cefThreadFunc(const std::string& initialUrl) {
    std::cout << "[CEF Thread] Starting..." << std::endl;

    // Inicializar CEF nesta thread
    CefMainArgs mainArgs;
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;

    if (!CefInitialize(mainArgs, settings, nullptr, nullptr)) {
        std::cerr << "[CEF Thread] CefInitialize failed!" << std::endl;
        m_running = false;
        return;
    }

    // Criar browser
    CefWindowInfo windowInfo;
    windowInfo.SetAsWindowless(nullptr);

    CefBrowserSettings browserSettings;

    std::string url = initialUrl.empty() ? "about:blank" : initialUrl;

    m_browser = CefBrowserHost::CreateBrowserSync(
        windowInfo, this, url, browserSettings, nullptr, nullptr);

    if (!m_browser) {
        std::cerr << "[CEF Thread] Browser creation failed!" << std::endl;
        CefShutdown();
        m_running = false;
        return;
    }

    m_cefReady = true;
    std::cout << "[CEF Thread] Ready, entering message loop" << std::endl;

    // Message loop do CEF
    while (m_running) {
        // Processar mensagens do CEF
        CefDoMessageLoopWork();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[CEF Thread] Exiting message loop" << std::endl;

    // Cleanup CEF
    if (m_browser) {
        m_browser->GetHost()->CloseBrowser(true);

        // Aguardar browser fechar
        int attempts = 0;
        while (m_browser && attempts < 100) {
            CefDoMessageLoopWork();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        m_browser = nullptr;
    }

    CefShutdown();
    std::cout << "[CEF Thread] Shutdown complete" << std::endl;
}

void UIRendererThreaded::updateTexture() {
    if (!m_needsTextureUpdate || !m_texture) return;

    std::lock_guard<std::mutex> lock(m_bufferMutex);

    if (!m_needsTextureUpdate) return; // Double check apos lock

    size_t dataSize = m_width * m_height * 4;
    void* pixelsCopy = malloc(dataSize);
    memcpy(pixelsCopy, m_pixelBuffer.data(), dataSize);

    filament::Texture::PixelBufferDescriptor pbd(
        pixelsCopy, dataSize,
        filament::Texture::Format::RGBA,
        filament::Texture::Type::UBYTE,
        [](void* buffer, size_t, void*) { free(buffer); });

    m_texture->setImage(*m_engine, 0, std::move(pbd));
    m_needsTextureUpdate = false;
}

void UIRendererThreaded::render(filament::Renderer* renderer) {
    if (m_view) {
        renderer->render(m_view);
    }
}

void UIRendererThreaded::loadUrl(const std::string& url) {
    if (m_browser) {
        m_browser->GetMainFrame()->LoadURL(url);
    }
}

void UIRendererThreaded::loadHtml(const std::string& html) {
    if (m_browser) {
        m_browser->GetMainFrame()->LoadURL("data:text/html;charset=utf-8," + html);
    }
}

void UIRendererThreaded::executeJavaScript(const std::string& code) {
    if (m_browser && m_browser->GetMainFrame()) {
        m_browser->GetMainFrame()->ExecuteJavaScript(code, "", 0);
    }
}

void UIRendererThreaded::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_pixelBuffer.resize(width * height * 4, 0);
    }

    // Recriar textura na thread principal
    if (m_texture) {
        m_engine->destroy(m_texture);
    }

    m_texture = filament::Texture::Builder()
        .width(width).height(height).levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .build(*m_engine);

    filament::TextureSampler sampler(
        filament::TextureSampler::MinFilter::LINEAR,
        filament::TextureSampler::MagFilter::LINEAR);
    m_materialInstance->setParameter("texture", m_texture, sampler);

    if (m_view) {
        m_view->setViewport(filament::Viewport(0, 0, width, height));
    }

    // Notificar CEF do resize
    if (m_browser) {
        m_browser->GetHost()->WasResized();
    }
}

// CefRenderHandler - chamado na thread do CEF
void UIRendererThreaded::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect(0, 0, m_width, m_height);
}

void UIRendererThreaded::OnPaint(CefRefPtr<CefBrowser> browser,
                                  PaintElementType type,
                                  const RectList& dirtyRects,
                                  const void* buffer,
                                  int width, int height) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);

    if (width == (int)m_width.load() && height == (int)m_height.load()) {
        // BGRA -> RGBA
        const uint8_t* src = static_cast<const uint8_t*>(buffer);
        size_t pixelCount = width * height;

        for (size_t i = 0; i < pixelCount; i++) {
            m_pixelBuffer[i * 4 + 0] = src[i * 4 + 2]; // R
            m_pixelBuffer[i * 4 + 1] = src[i * 4 + 1]; // G
            m_pixelBuffer[i * 4 + 2] = src[i * 4 + 0]; // B
            m_pixelBuffer[i * 4 + 3] = src[i * 4 + 3]; // A
        }
        m_needsTextureUpdate = true;
    }
}

void UIRendererThreaded::createFilamentResources() {
    createMaterial();
    createQuad();

    m_scene = m_engine->createScene();
    m_view = m_engine->createView();

    utils::EntityManager& em = utils::EntityManager::get();
    m_cameraEntity = em.create();
    m_camera = m_engine->createCamera(m_cameraEntity);

    m_camera->setProjection(filament::Camera::Projection::ORTHO,
        0, 1, 0, 1, -1, 1);

    m_view->setCamera(m_camera);
    m_view->setScene(m_scene);
    m_view->setViewport(filament::Viewport(0, 0, m_width, m_height));
    m_view->setPostProcessingEnabled(false);
    m_view->setBlendMode(filament::View::BlendMode::TRANSLUCENT);

    m_scene->addEntity(m_quadEntity);
}

void UIRendererThreaded::createMaterial() {
    std::string matPath = "D:/Workspace/LiteEngine/core/resources/filament/materials/ui_overlay.filamat";
    std::ifstream file(matPath, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "[UIRendererThreaded] Material not found: " << matPath << std::endl;
        return;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    file.close();

    m_material = filament::Material::Builder()
        .package(data.data(), data.size())
        .build(*m_engine);

    m_materialInstance = m_material->createInstance();

    m_texture = filament::Texture::Builder()
        .width(m_width).height(m_height).levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .build(*m_engine);

    filament::TextureSampler sampler(
        filament::TextureSampler::MinFilter::LINEAR,
        filament::TextureSampler::MagFilter::LINEAR);
    m_materialInstance->setParameter("texture", m_texture, sampler);
}

void UIRendererThreaded::createQuad() {
    struct Vertex {
        filament::math::float3 position;
        filament::math::float2 uv;
    };

    static const Vertex vertices[] = {
        {{0, 0, 0}, {0, 0}},
        {{1, 0, 0}, {1, 0}},
        {{1, 1, 0}, {1, 1}},
        {{0, 1, 0}, {0, 1}},
    };

    static const uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    m_vertexBuffer = filament::VertexBuffer::Builder()
        .vertexCount(4)
        .bufferCount(1)
        .attribute(filament::VertexAttribute::POSITION, 0,
                   filament::VertexBuffer::AttributeType::FLOAT3, 0, sizeof(Vertex))
        .attribute(filament::VertexAttribute::UV0, 0,
                   filament::VertexBuffer::AttributeType::FLOAT2,
                   offsetof(Vertex, uv), sizeof(Vertex))
        .build(*m_engine);

    m_vertexBuffer->setBufferAt(*m_engine, 0,
        filament::VertexBuffer::BufferDescriptor(vertices, sizeof(vertices)));

    m_indexBuffer = filament::IndexBuffer::Builder()
        .indexCount(6)
        .bufferType(filament::IndexBuffer::IndexType::USHORT)
        .build(*m_engine);

    m_indexBuffer->setBuffer(*m_engine,
        filament::IndexBuffer::BufferDescriptor(indices, sizeof(indices)));

    utils::EntityManager& em = utils::EntityManager::get();
    m_quadEntity = em.create();

    filament::RenderableManager::Builder(1)
        .boundingBox({{0, 0, 0}, {1, 1, 0.1f}})
        .material(0, m_materialInstance)
        .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
                  m_vertexBuffer, m_indexBuffer, 0, 6)
        .culling(false)
        .receiveShadows(false)
        .castShadows(false)
        .build(*m_engine, m_quadEntity);
}

} // namespace lite
