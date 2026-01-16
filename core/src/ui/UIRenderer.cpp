// IMPORTANTE: Definir NOMINMAX antes de qualquer include para evitar conflito com CEF
#define NOMINMAX

#include <core/ui/UIRenderer.h>
#include <fstream>
#include <iostream>
#include <filament/TextureSampler.h>
#include <filament/Viewport.h>
#include <math/vec3.h>
#include <math/vec2.h>

namespace lite {

UIRenderer::UIRenderer(filament::Engine* engine, uint32_t width, uint32_t height)
    : m_engine(engine), m_width(width), m_height(height)
{
    m_pixelBuffer.resize(width * height * 4, 0);
}

UIRenderer::~UIRenderer() {
    shutdown();
}

bool UIRenderer::initialize(const std::string& initialUrl) {
    std::cout << "[UIRenderer] Initializing..." << std::endl;

    // Inicializar CEF
    CefMainArgs mainArgs;
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;

    if (!CefInitialize(mainArgs, settings, nullptr, nullptr)) {
        std::cerr << "[UIRenderer] CEF init failed!" << std::endl;
        return false;
    }

    // Browser windowless
    CefWindowInfo windowInfo;
    windowInfo.SetAsWindowless(nullptr);

    CefBrowserSettings browserSettings;
    browserSettings.windowless_frame_rate = 60;

    std::string url = initialUrl.empty() ? "about:blank" : initialUrl;

    m_browser = CefBrowserHost::CreateBrowserSync(
        windowInfo, this, url, browserSettings, nullptr, nullptr);

    if (!m_browser) {
        std::cerr << "[UIRenderer] Browser creation failed!" << std::endl;
        return false;
    }

    // Criar recursos Filament
    createMaterial();
    createQuad();

    // View/Scene para UI
    m_scene = m_engine->createScene();
    m_view = m_engine->createView();

    utils::EntityManager& em = utils::EntityManager::get();
    m_cameraEntity = em.create();
    m_camera = m_engine->createCamera(m_cameraEntity);

    // Camera ortografica
    m_camera->setProjection(filament::Camera::Projection::ORTHO,
        0, 1, 0, 1, -1, 1);

    m_view->setCamera(m_camera);
    m_view->setScene(m_scene);
    m_view->setViewport(filament::Viewport(0, 0, m_width, m_height));
    m_view->setPostProcessingEnabled(false);
    m_view->setBlendMode(filament::View::BlendMode::TRANSLUCENT);

    m_scene->addEntity(m_quadEntity);

    std::cout << "[UIRenderer] Ready" << std::endl;
    return true;
}

void UIRenderer::shutdown() {
    if (m_browser) {
        m_browser->GetHost()->CloseBrowser(true);
        m_browser = nullptr;
    }

    CefShutdown();

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
}

void UIRenderer::update() {
    CefDoMessageLoopWork();
    updateTexture();
}

void UIRenderer::render(filament::Renderer* renderer) {
    if (m_view) {
        renderer->render(m_view);
    }
}

void UIRenderer::loadUrl(const std::string& url) {
    if (m_browser) {
        m_browser->GetMainFrame()->LoadURL(url);
    }
}

void UIRenderer::loadHtml(const std::string& html) {
    if (m_browser) {
        m_browser->GetMainFrame()->LoadURL("data:text/html;charset=utf-8," + html);
    }
}

void UIRenderer::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_pixelBuffer.resize(width * height * 4, 0);
    }

    if (m_browser) m_browser->GetHost()->WasResized();
    if (m_view) m_view->setViewport(filament::Viewport(0, 0, width, height));

    // Recriar textura
    if (m_texture) {
        m_engine->destroy(m_texture);
    }

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

// CefRenderHandler
void UIRenderer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect(0, 0, m_width, m_height);
}

void UIRenderer::OnPaint(CefRefPtr<CefBrowser> browser,
                         PaintElementType type,
                         const RectList& dirtyRects,
                         const void* buffer,
                         int width, int height) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);

    if (width == (int)m_width && height == (int)m_height) {
        // BGRA -> RGBA
        const uint8_t* src = static_cast<const uint8_t*>(buffer);
        for (size_t i = 0; i < m_width * m_height; i++) {
            m_pixelBuffer[i * 4 + 0] = src[i * 4 + 2]; // R
            m_pixelBuffer[i * 4 + 1] = src[i * 4 + 1]; // G
            m_pixelBuffer[i * 4 + 2] = src[i * 4 + 0]; // B
            m_pixelBuffer[i * 4 + 3] = src[i * 4 + 3]; // A
        }
        m_needsTextureUpdate = true;
    }
}

void UIRenderer::createMaterial() {
    std::string matPath = "D:/Workspace/LiteEngine/core/resources/filament/materials/ui_overlay.filamat";
    std::ifstream file(matPath, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "[UIRenderer] Material not found: " << matPath << std::endl;
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

void UIRenderer::createQuad() {
    struct Vertex {
        filament::math::float3 position;
        filament::math::float2 uv;
    };

    // UV: CEF tem origem top-left, Filament tem origem bottom-left
    // Não inverter V para corrigir orientação
    static const Vertex vertices[] = {
        {{0, 0, 0}, {0, 0}},  // bottom-left  -> top-left da textura
        {{1, 0, 0}, {1, 0}},  // bottom-right -> top-right da textura
        {{1, 1, 0}, {1, 1}},  // top-right    -> bottom-right da textura
        {{0, 1, 0}, {0, 1}},  // top-left     -> bottom-left da textura
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

void UIRenderer::updateTexture() {
    std::lock_guard<std::mutex> lock(m_bufferMutex);

    if (!m_needsTextureUpdate || !m_texture) return;

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

} // namespace lite
