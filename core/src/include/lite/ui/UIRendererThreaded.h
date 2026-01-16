#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>

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

namespace lite {

class UIRendererThreaded : public CefClient, public CefRenderHandler {
public:
    UIRendererThreaded(filament::Engine* engine, uint32_t width, uint32_t height);
    ~UIRendererThreaded();

    // Inicia thread do CEF (chamado apos Filament estar pronto)
    bool start(const std::string& initialUrl = "");

    // Para thread do CEF (chamado antes de destruir Filament)
    void stop();

    // Chamado pela thread principal a cada frame
    void updateTexture();
    void render(filament::Renderer* renderer);

    // Comandos enfileirados para a thread do CEF
    void loadUrl(const std::string& url);
    void loadHtml(const std::string& html);
    void resize(uint32_t width, uint32_t height);
    void executeJavaScript(const std::string& code);

    filament::View* getView() const { return m_view; }

    // CefClient
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

    // CefRenderHandler (chamados na thread do CEF)
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width, int height) override;

private:
    // Funcao da thread do CEF
    void cefThreadFunc(const std::string& initialUrl);

    // Criacao de recursos Filament (thread principal)
    void createFilamentResources();
    void createQuad();
    void createMaterial();

    // Filament (apenas thread principal)
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

    // CEF (apenas thread do CEF)
    CefRefPtr<CefBrowser> m_browser;

    // Buffer compartilhado entre threads
    std::vector<uint8_t> m_pixelBuffer;
    std::mutex m_bufferMutex;
    std::atomic<bool> m_needsTextureUpdate{false};

    IMPLEMENT_REFCOUNTING(UIRendererThreaded);
};

} // namespace lite
