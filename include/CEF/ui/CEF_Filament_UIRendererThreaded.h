#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <array>
#include <map>
#include <functional>

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

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>

    #undef interface
    #undef min
    #undef max
#endif

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_load_handler.h"
#include "include/wrapper/cef_message_router.h"

#include <core/UI/UIRenderer.h>
#include <core/ui/elements/UIElements.h>

namespace lite {

class CEF_Filament_UIRendererThreaded :
        public CefClient
    ,   public CefRenderHandler
    ,   public CefLifeSpanHandler
    ,   public CefDisplayHandler
    ,   public CefLoadHandler
    ,   public CefMessageRouterBrowserSide::Handler
    ,   public UIRenderer<filament::Renderer> {
public:
    CEF_Filament_UIRendererThreaded(filament::Engine* engine, uint32_t width, uint32_t height);
    ~CEF_Filament_UIRendererThreaded();

    virtual bool start() override;
    virtual bool stop() override;

    virtual void update();
    virtual void render(filament::Renderer* renderer);
    virtual void sendInputEvent(const InputEvent& event) override;

    void loadUrl(const std::string& url);
    void loadHtml(const std::string& html);
    void resize(uint32_t width, uint32_t height);
    void executeJavaScript(const std::string& code);

    // Nova: versao com throttle para chamadas frequentes
    void executeJavaScriptThrottled(const std::string& code, uint32_t minIntervalMs = 16);

    filament::View* getView() const { return m_view; }

    bool isPageLoaded() const { return m_pageLoaded; }

    // CefClient
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

    // CefClient - MessageRouter IPC
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefProcessId source_process,
                                   CefRefPtr<CefProcessMessage> message) override;

    // CefLifeSpanHandler
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefDisplayHandler
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                          cef_log_severity_t level,
                          const CefString& message,
                          const CefString& source,
                          int line) override;

    // CefLoadHandler
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;

    // CefRenderHandler
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width, int height) override;

    // CefMessageRouterBrowserSide::Handler
    bool OnQuery(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int64_t query_id,
                 const CefString& request,
                 bool persistent,
                 CefRefPtr<CefMessageRouterBrowserSide::Callback> callback) override;

private:
    void cefThreadFunc(const std::string& initialUrl);
    void createFilamentResources();
    void createQuad();
    void createMaterial();
    void processModifiers(int32_t modifier, lite::InputEvent event, std::initializer_list<INPUT_KEYS> keys);

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
    std::atomic<bool> m_uiAppReady{false};

    // CEF
    CefRefPtr<CefBrowser> m_browser;
    std::atomic<bool> m_browserClosed{false};
    std::atomic<bool> m_pageLoaded{false};

    // MessageRouter browser side
    CefRefPtr<CefMessageRouterBrowserSide> m_messageRouter;

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

    //TODO: TURN IN MORE GENERIC?
    std::map<std::string, std::function<void(int)>> cef_events;

    uint32_t m_currentModifiers = 0;
    
    IMPLEMENT_REFCOUNTING(CEF_Filament_UIRendererThreaded);
};

} // namespace lite
