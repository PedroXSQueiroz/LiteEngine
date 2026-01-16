#pragma once

#include <vector>
#include <mutex>
#include <string>

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

class UIRenderer : public CefClient, public CefRenderHandler {
public:
    UIRenderer(filament::Engine* engine, uint32_t width, uint32_t height);
    ~UIRenderer();

    bool initialize(const std::string& initialUrl = "");
    void shutdown();
    void update();
    void render(filament::Renderer* renderer);

    void loadUrl(const std::string& url);
    void loadHtml(const std::string& html);
    void resize(uint32_t width, uint32_t height);

    filament::View* getView() const { return m_view; }

    // CefClient
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

    // CefRenderHandler
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width, int height) override;

private:
    void createQuad();
    void createMaterial();
    void updateTexture();

    filament::Engine* m_engine;
    uint32_t m_width;
    uint32_t m_height;

    // Filament
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

    // CEF
    CefRefPtr<CefBrowser> m_browser;

    // Pixels
    std::vector<uint8_t> m_pixelBuffer;
    std::mutex m_bufferMutex;
    bool m_needsTextureUpdate = false;

    IMPLEMENT_REFCOUNTING(UIRenderer);
};

} // namespace lite
