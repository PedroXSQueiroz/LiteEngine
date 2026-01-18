#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <array>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <filament/Camera.h>
#include <filament/Renderer.h>
#include <filament/Texture.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/RenderableManager.h>
#include <utils/EntityManager.h>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/cef_life_span_handler.h"

namespace lite {

class UIRendererThreaded : public CefClient, public CefRenderHandler, public CefLifeSpanHandler {
public:
    UIRendererThreaded(filament::Engine* engine, uint32_t width, uint32_t height);
    ~UIRendererThreaded();

    bool start(const std::string& initialUrl = "");
    void stop();

    void updateTexture();
    void render(filament::Renderer* renderer);

    void loadUrl(const std::string& url);
    void loadHtml(const std::string& html);
    void resize(uint32_t width, uint32_t height);
    void executeJavaScript(const std::string& code);

    // Nova: versao com throttle para chamadas frequentes
    void executeJavaScriptThrottled(const std::string& code, uint32_t minIntervalMs = 16);

    filament::View* getView() const { return m_view; }

    // CefClient
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

    // CefLifeSpanHandler
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefRenderHandler
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width, int height) override;

private:
    void cefThreadFunc(const std::string& initialUrl);
    void createFilamentResources();
    void createQuad();
    void createMaterial();

    // Nova: conversao otimizada BGRA->RGBA
    void convertBGRAtoRGBA(const uint8_t* src, uint8_t* dst, size_t pixelCount);

    // Filament resources
    filament::Engine* m_engine;
    filament::View* m_view = nullptr;
    filament::Scene* m_scene = nullptr;
    filament::Camera* m_camera = nullptr;
    utils::Entity m_cameraEntity;
    filament::Texture* m_texture = nullptr;
    filament::Material* m_material = nullptr;
    filament::MaterialInstance* m_materialInstance = nullptr;
    filament::VertexBuffer* m_vertexBuffer = nullptr;
    filament::IndexBuffer* m_indexBuffer = nullptr;
    utils::Entity m_quadEntity;

    // Dimensoes
    std::atomic<uint32_t> m_width;
    std::atomic<uint32_t> m_height;

    // Thread do CEF
    std::thread m_cefThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cefReady{false};

    // CEF
    CefRefPtr<CefBrowser> m_browser;
    std::atomic<bool> m_browserClosed{false};

    // ========== OTIMIZACAO 1: Double Buffering ==========
    // Dois buffers para evitar lock contention
    static constexpr size_t BUFFER_COUNT = 2;
    std::array<std::vector<uint8_t>, BUFFER_COUNT> m_pixelBuffers;
    std::atomic<size_t> m_writeBufferIndex{0};  // Buffer sendo escrito pelo CEF
    std::atomic<size_t> m_readBufferIndex{1};   // Buffer sendo lido pelo Filament
    std::mutex m_swapMutex;  // Apenas para swap, nao para copia
    std::atomic<bool> m_needsTextureUpdate{false};

    // ========== OTIMIZACAO 2: Pool de memoria para upload ==========
    // Buffer pre-alocado para upload de textura (evita malloc/free por frame)
    std::vector<uint8_t> m_uploadBuffer;

    // ========== OTIMIZACAO 3: Throttle para JS ==========
    std::chrono::steady_clock::time_point m_lastJsCallTime;

    IMPLEMENT_REFCOUNTING(UIRendererThreaded);
};

} // namespace lite