// IMPORTANTE: Definir NOMINMAX antes de qualquer include
#define NOMINMAX

#include <core/ui/UIRendererThreaded.h>
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
    size_t bufferSize = width * height * 4;

    // Inicializar double buffers
    for (auto& buffer : m_pixelBuffers) {
        buffer.resize(bufferSize, 0);
    }

    // Pre-alocar buffer de upload
    m_uploadBuffer.resize(bufferSize);

    m_lastJsCallTime = std::chrono::steady_clock::now();
}

UIRendererThreaded::~UIRendererThreaded() {
    stop();
}

bool UIRendererThreaded::start(const std::string& initialUrl) {
    std::cout << "[UIRendererThreaded] Starting..." << std::endl;

    createFilamentResources();

    m_running = true;
    m_cefThread = std::thread(&UIRendererThreaded::cefThreadFunc, this, initialUrl);

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

    CefMainArgs mainArgs;
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = true;

    if (!CefInitialize(mainArgs, settings, nullptr, nullptr)) {
        std::cerr << "[CEF Thread] CefInitialize failed!" << std::endl;
        m_running = false;
        return;
    }

    std::cout << "[CEF Thread] CEF initialized, creating browser..." << std::endl;

    CefWindowInfo windowInfo;
    windowInfo.SetAsWindowless(nullptr);

    CefBrowserSettings browserSettings;

    std::string url = initialUrl.empty() ? "about:blank" : initialUrl;

    bool created = CefBrowserHost::CreateBrowser(
        windowInfo, this, url, browserSettings, nullptr, nullptr);

    if (!created) {
        std::cerr << "[CEF Thread] Browser creation request failed!" << std::endl;
        CefShutdown();
        m_running = false;
        return;
    }

    std::cout << "[CEF Thread] Browser creation requested, waiting..." << std::endl;

    // Com multi_threaded_message_loop = true, aguardar sinal de parada
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[CEF Thread] Stopping, closing browser..." << std::endl;

    if (m_browser) {
        m_browser->GetHost()->CloseBrowser(true);

        int attempts = 0;
        while (!m_browserClosed && attempts < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }

        if (!m_browserClosed) {
            std::cerr << "[CEF Thread] Browser close timeout" << std::endl;
        }

        m_browser = nullptr;
    }

    std::cout << "[CEF Thread] Shutting down CEF..." << std::endl;
    CefShutdown();
    std::cout << "[CEF Thread] Shutdown complete" << std::endl;
}

// CefLifeSpanHandler
void UIRendererThreaded::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    std::cout << "[CEF] OnAfterCreated - Browser ready" << std::endl;
    m_browser = browser;
    m_cefReady = true;
}

void UIRendererThreaded::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    std::cout << "[CEF] OnBeforeClose" << std::endl;
    m_browserClosed = true;
    m_browser = nullptr;
}

// ========== OTIMIZACAO 4: Conversao BGRA->RGBA otimizada ==========
void UIRendererThreaded::convertBGRAtoRGBA(const uint8_t* src, uint8_t* dst, size_t pixelCount) {
    // Processar 4 pixels por vez (16 bytes) quando possivel
    size_t i = 0;

    // Processar em blocos de 4 pixels
    size_t blockCount = pixelCount / 4;
    for (size_t block = 0; block < blockCount; ++block) {
        size_t base = block * 16;

        // Pixel 0
        dst[base + 0] = src[base + 2];  // R
        dst[base + 1] = src[base + 1];  // G
        dst[base + 2] = src[base + 0];  // B
        dst[base + 3] = src[base + 3];  // A

        // Pixel 1
        dst[base + 4] = src[base + 6];
        dst[base + 5] = src[base + 5];
        dst[base + 6] = src[base + 4];
        dst[base + 7] = src[base + 7];

        // Pixel 2
        dst[base + 8] = src[base + 10];
        dst[base + 9] = src[base + 9];
        dst[base + 10] = src[base + 8];
        dst[base + 11] = src[base + 11];

        // Pixel 3
        dst[base + 12] = src[base + 14];
        dst[base + 13] = src[base + 13];
        dst[base + 14] = src[base + 12];
        dst[base + 15] = src[base + 15];

        i += 4;
    }

    // Processar pixels restantes
    for (; i < pixelCount; ++i) {
        size_t idx = i * 4;
        dst[idx + 0] = src[idx + 2];  // R
        dst[idx + 1] = src[idx + 1];  // G
        dst[idx + 2] = src[idx + 0];  // B
        dst[idx + 3] = src[idx + 3];  // A
    }
}

// ========== OTIMIZACAO 1 & 4: OnPaint com double buffer ==========
void UIRendererThreaded::OnPaint(CefRefPtr<CefBrowser> browser,
                                  PaintElementType type,
                                  const RectList& dirtyRects,
                                  const void* buffer,
                                  int width, int height) {
    if (width != (int)m_width.load() || height != (int)m_height.load()) {
        return;
    }

    size_t writeIdx = m_writeBufferIndex.load();
    auto& writeBuffer = m_pixelBuffers[writeIdx];

    // Conversao BGRA->RGBA diretamente no write buffer (sem lock!)
    const uint8_t* src = static_cast<const uint8_t*>(buffer);
    convertBGRAtoRGBA(src, writeBuffer.data(), width * height);

    // Swap buffers (lock minimo apenas para trocar indices)
    {
        std::lock_guard<std::mutex> lock(m_swapMutex);
        size_t newWriteIdx = m_readBufferIndex.load();
        m_readBufferIndex.store(writeIdx);
        m_writeBufferIndex.store(newWriteIdx);
    }

    m_needsTextureUpdate = true;
}

// ========== OTIMIZACAO 2: updateTexture sem malloc ==========
void UIRendererThreaded::updateTexture() {
    if (!m_needsTextureUpdate || !m_texture) return;

    // Copiar do read buffer para upload buffer (lock minimo)
    {
        std::lock_guard<std::mutex> lock(m_swapMutex);
        size_t readIdx = m_readBufferIndex.load();
        memcpy(m_uploadBuffer.data(), m_pixelBuffers[readIdx].data(), m_uploadBuffer.size());
    }

    m_needsTextureUpdate = false;

    // Upload para GPU usando buffer pre-alocado
    // Nota: Filament faz copia interna, entao podemos reusar m_uploadBuffer
    filament::Texture::PixelBufferDescriptor pbd(
        m_uploadBuffer.data(),
        m_uploadBuffer.size(),
        filament::Texture::Format::RGBA,
        filament::Texture::Type::UBYTE);

    m_texture->setImage(*m_engine, 0, std::move(pbd));
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

// ========== OTIMIZACAO 3: executeJavaScript com throttle ==========
void UIRendererThreaded::executeJavaScriptThrottled(const std::string& code, uint32_t minIntervalMs) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastJsCallTime).count();

    if (elapsed >= minIntervalMs) {
        executeJavaScript(code);
        m_lastJsCallTime = now;
    }
}

void UIRendererThreaded::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    size_t bufferSize = width * height * 4;

    // Redimensionar todos os buffers
    {
        std::lock_guard<std::mutex> lock(m_swapMutex);
        for (auto& buffer : m_pixelBuffers) {
            buffer.resize(bufferSize, 0);
        }
        m_uploadBuffer.resize(bufferSize);
    }

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

    if (m_browser) {
        m_browser->GetHost()->WasResized();
    }
}

void UIRendererThreaded::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect(0, 0, m_width, m_height);
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
