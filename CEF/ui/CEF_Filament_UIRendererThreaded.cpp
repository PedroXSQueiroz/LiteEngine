// IMPORTANTE: Definir NOMINMAX antes de qualquer include
#define NOMINMAX

#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>
#include <CEF/ui/elements/CEF_UIElements.h>
#include <CEF/CEF_UIApp.h>

#include <fstream>
#include <iostream>
#include <filament/TextureSampler.h>
#include <filament/Viewport.h>
#include <math/vec3.h>
#include <math/vec2.h>
// Filament macros collide with nlohmann/json internals
#undef assert_invariant
#undef UTILS_VERY_LIKELY
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lite {

CEF_Filament_UIRendererThreaded::CEF_Filament_UIRendererThreaded(filament::Engine* engine, uint32_t width, uint32_t height)
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

CEF_Filament_UIRendererThreaded::~CEF_Filament_UIRendererThreaded() {
    stop();
}

bool CEF_Filament_UIRendererThreaded::start() {
    
    // std::string initialUrl = "file:///D:/Workspace/LiteEngine/CEF/ui/resources/index.html";
    std::string initialUrl = "file:///D:/Workspace/LiteEngine/CEF/ui/resources/cef-ui/dist/index.html";
    
    std::cout << "[UIRendererThreaded] Starting..." << std::endl;

    this->cef_events.emplace("ui_ready", [&](int uiElementId){
        this->m_uiAppReady = true;
    });
    
    createFilamentResources();

    m_running = true;
    m_cefThread = std::thread(&CEF_Filament_UIRendererThreaded::cefThreadFunc, this, initialUrl);

    int attempts = 0;
    while ((!m_cefReady || !m_uiAppReady)  && m_running && attempts < 500) {
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

bool CEF_Filament_UIRendererThreaded::stop() {
    if (!m_running) return false;

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

    return true;
}

void CEF_Filament_UIRendererThreaded::cefThreadFunc(const std::string& initialUrl) {
    std::cout << "[CEF Thread] Starting..." << std::endl;

    CefMainArgs mainArgs;
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = true;

    CefRefPtr<CEF_UIApp> app = new CEF_UIApp();
    if (!CefInitialize(mainArgs, settings, app.get(), nullptr)) {
        std::cerr << "[CEF Thread] CefInitialize failed!" << std::endl;
        m_running = false;
        return;
    }

    std::cout << "[CEF Thread] CEF initialized, creating browser..." << std::endl;

    CefWindowInfo windowInfo;
    windowInfo.SetAsWindowless(nullptr);

    CefBrowserSettings browserSettings;
    browserSettings.background_color = CefColorSetARGB(0, 0, 0, 0);

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
void CEF_Filament_UIRendererThreaded::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    std::cout << "[CEF] OnAfterCreated - Browser ready" << std::endl;
    m_browser = browser;

    // Create MessageRouter browser side
    CefMessageRouterConfig config;
    m_messageRouter = CefMessageRouterBrowserSide::Create(config);
    m_messageRouter->AddHandler(this, false);

    m_cefReady = true;
}

void CEF_Filament_UIRendererThreaded::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    std::cout << "[CEF] OnBeforeClose" << std::endl;
    if (m_messageRouter) {
        m_messageRouter->OnBeforeClose(browser);
    }
    m_browserClosed = true;
    m_browser = nullptr;
}

// CefDisplayHandler
bool CEF_Filament_UIRendererThreaded::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                                        cef_log_severity_t level,
                                                        const CefString& message,
                                                        const CefString& source,
                                                        int line) {
    const char* levelStr = "INFO";
    if (level == LOGSEVERITY_WARNING) levelStr = "WARN";
    else if (level >= LOGSEVERITY_ERROR) levelStr = "ERROR";

    std::cout << "[CEF JS " << levelStr << "] " << message.ToString()
              << " (" << source.ToString() << ":" << line << ")" << std::endl;
    return false;
}

// CefLoadHandler
void CEF_Filament_UIRendererThreaded::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                                                  CefRefPtr<CefFrame> frame,
                                                  int httpStatusCode) {
    if (frame->IsMain()) {
        std::cout << "[CEF] Page loaded (status: " << httpStatusCode << ")" << std::endl;
        m_pageLoaded = true;
    }
}

// ========== OTIMIZACAO 4: Conversao BGRA->RGBA otimizada ==========
void CEF_Filament_UIRendererThreaded::convertBGRAtoRGBA(const uint8_t* src, uint8_t* dst, size_t pixelCount) {
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

// CefClient - MessageRouter IPC
bool CEF_Filament_UIRendererThreaded::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
    if (m_messageRouter) {
        return m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message);
    }
    return false;
}

// CefMessageRouterBrowserSide::Handler
bool CEF_Filament_UIRendererThreaded::OnQuery(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int64_t query_id,
    const CefString& request,
    bool persistent,
    CefRefPtr<CefMessageRouterBrowserSide::Callback> callback) {

    std::string req = request.ToString();
    std::cout << "[CEF Query] " << req << std::endl;

    auto jsonRequest = json::parse(req);

    if(jsonRequest["event"] == "ui_ready")
    {
        this->cef_events["ui_ready"](-1);
    }

    if(     jsonRequest.contains("id")
        &&  jsonRequest.contains("type")
        &&  this->m_uiElements.contains(jsonRequest["id"])){
        std::string value = jsonRequest.contains("value") ? jsonRequest["value"].get<std::string>() : "";
        this->m_uiElements.at(jsonRequest["id"]).invokeEvents<UIRenderer<filament::Renderer>>(jsonRequest["type"], value);
    }
    
    // TODO: Parse request JSON and handle actions
    callback->Success("ok");
    return true;
}

// ========== OTIMIZACAO 1 & 4: OnPaint com double buffer ==========
void CEF_Filament_UIRendererThreaded::OnPaint(CefRefPtr<CefBrowser> browser,
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
void CEF_Filament_UIRendererThreaded::update() {
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

void CEF_Filament_UIRendererThreaded::render(filament::Renderer* renderer) {
    if (m_view) {
        renderer->render(m_view);
    }
}

void CEF_Filament_UIRendererThreaded::loadUrl(const std::string& url) {
    if (m_browser) {
        m_browser->GetMainFrame()->LoadURL(url);
    }
}

void CEF_Filament_UIRendererThreaded::loadHtml(const std::string& html) {
    if (m_browser) {
        m_browser->GetMainFrame()->LoadURL("data:text/html;charset=utf-8," + html);
    }
}

void CEF_Filament_UIRendererThreaded::executeJavaScript(const std::string& code) {
    if (m_browser && m_browser->GetMainFrame()) {
        m_browser->GetMainFrame()->ExecuteJavaScript(code, "", 0);
    }
}

// ========== OTIMIZACAO 3: executeJavaScript com throttle ==========
void CEF_Filament_UIRendererThreaded::executeJavaScriptThrottled(const std::string& code, uint32_t minIntervalMs) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastJsCallTime).count();

    if (elapsed >= minIntervalMs) {
        executeJavaScript(code);
        m_lastJsCallTime = now;
    }
}

void CEF_Filament_UIRendererThreaded::resize(uint32_t width, uint32_t height) {
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

void CEF_Filament_UIRendererThreaded::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect(0, 0, m_width, m_height);
}

void CEF_Filament_UIRendererThreaded::createFilamentResources() {
    createMaterial();
    createQuad();

    //TODO: PODERIA SER USADA A MESMA VIEW QUE JÁ É USADA PARA O RESTO E ASSIM NÃO PRECISANDO CHAMAR MAIS UM RENDER?
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

void CEF_Filament_UIRendererThreaded::createMaterial() {
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

void CEF_Filament_UIRendererThreaded::processModifiers(int32_t modifier, lite::InputEvent event,  std::initializer_list<INPUT_KEYS> keys)
{
    auto extractKeyModifier = [](std::initializer_list<INPUT_KEYS> inputedKeys, lite::InputEvent currentEvent){
        
        for(INPUT_KEYS currentKey : inputedKeys)
        {
            if(currentEvent.keys.contains(currentKey))
            {
                return currentEvent.keys.at(currentKey);
            }
        }

        return INPUT_KEY_STATES::NONE;
    };
    
    INPUT_KEY_STATES modifierKeyState = extractKeyModifier(keys, event);

    if (modifierKeyState == INPUT_KEY_STATES::DOWN || modifierKeyState == INPUT_KEY_STATES::PRESSED)
    {
        this->m_currentModifiers |= modifier;
    }
    else if (modifierKeyState == INPUT_KEY_STATES::UP)
    {
        this->m_currentModifiers &= ~modifier;
    }
    // NONE = tecla ausente neste frame, manter estado anterior
    
}

void CEF_Filament_UIRendererThreaded::createQuad() {
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

void CEF_Filament_UIRendererThreaded::sendInputEvent(const InputEvent& event) {
    if (!m_browser) return;
    auto host = m_browser->GetHost();

    // Mouse position
    auto mouseIt = event.analogs.find(INPUT_ANALOGS::MOUSE);
    int mouseX = 0, mouseY = 0;
    if (mouseIt != event.analogs.end()) {
        mouseX = static_cast<int>(mouseIt->second.x);
        mouseY = static_cast<int>(mouseIt->second.y);

        CefMouseEvent cefEvent;
        cefEvent.x = mouseX;
        cefEvent.y = mouseY;
        cefEvent.modifiers = 0;
        host->SendMouseMoveEvent(cefEvent, false);
    }

    // Mouse wheel
    auto wheelIt = event.analogs.find(INPUT_ANALOGS::MOUSE_WHEEL);
    if (wheelIt != event.analogs.end()) {
        CefMouseEvent cefEvent;
        cefEvent.x = mouseX;
        cefEvent.y = mouseY;
        cefEvent.modifiers = 0;
        host->SendMouseWheelEvent(cefEvent,
            static_cast<int>(wheelIt->second.x),
            static_cast<int>(wheelIt->second.y));
    }

    // Calcular modifiers a partir do mapa de keys
    // uint32_t modifiers = 0;
    // if (event.keys.contains(INPUT_KEYS::KEY_LSHIFT) || event.keys.contains(INPUT_KEYS::KEY_RSHIFT))
    // {
    //     bool addModifier = (    ( event.keys.contains(INPUT_KEYS::KEY_LSHIFT) && event.keys.at(INPUT_KEYS::KEY_LSHIFT) == INPUT_KEY_STATES::DOWN ) 
    //                         &&  ( event.keys.contains(INPUT_KEYS::KEY_RSHIFT) && event.keys.at(INPUT_KEYS::KEY_RSHIFT) == INPUT_KEY_STATES::DOWN ) 
    //                         );
        
    //     if(addModifier)
    //         this->m_currentModifiers |= EVENTFLAG_SHIFT_DOWN;
    //     else
    //         this->m_currentModifiers &= ~EVENTFLAG_SHIFT_DOWN;
    // }

    processModifiers(EVENTFLAG_SHIFT_DOWN, event, { INPUT_KEYS::KEY_LSHIFT, INPUT_KEYS::KEY_RSHIFT });
    processModifiers(EVENTFLAG_CONTROL_DOWN, event, { INPUT_KEYS::KEY_LCTRL, INPUT_KEYS::KEY_RCTRL });
    processModifiers(EVENTFLAG_ALT_DOWN, event, { INPUT_KEYS::KEY_LALT, INPUT_KEYS::KEY_RALT });

    if(this->m_currentModifiers & EVENTFLAG_SHIFT_DOWN) std::cout << "[UIRendererThreaded] MODIFIER SHIFT ON" << std::endl;
    if(this->m_currentModifiers & EVENTFLAG_CONTROL_DOWN) std::cout << "[UIRendererThreaded] MODIFIER CONTROL ON" << std::endl;
    if(this->m_currentModifiers & EVENTFLAG_ALT_DOWN) std::cout << "[UIRendererThreaded] MODIFIER ALT ON" << std::endl;
    
    // if (event.keys.contains(INPUT_KEYS::KEY_LCTRL) || event.keys.contains(INPUT_KEYS::KEY_RCTRL))
    // {
    //     this->m_currentModifiers |= EVENTFLAG_CONTROL_DOWN;
    // }
    
    // if (event.keys.contains(INPUT_KEYS::KEY_LALT) || event.keys.contains(INPUT_KEYS::KEY_RALT))
    // {
    //     this->m_currentModifiers |= EVENTFLAG_ALT_DOWN;
    // }

    // Mouse buttons
    auto sendMouseBtn = [&](INPUT_KEYS key, CefBrowserHost::MouseButtonType btn) {
        auto it = event.keys.find(key);
        if (it == event.keys.end()) return;

        CefMouseEvent cefEvent;
        cefEvent.x = mouseX;
        cefEvent.y = mouseY;
        cefEvent.modifiers = m_currentModifiers;

        if (it->second == INPUT_KEY_STATES::DOWN)
            host->SendMouseClickEvent(cefEvent, btn, false, 1);
        else if (it->second == INPUT_KEY_STATES::UP)
            host->SendMouseClickEvent(cefEvent, btn, true, 1);
    };

    sendMouseBtn(INPUT_KEYS::MOUSE_LEFT, MBT_LEFT);
    sendMouseBtn(INPUT_KEYS::MOUSE_RIGHT, MBT_RIGHT);
    sendMouseBtn(INPUT_KEYS::MOUSE_MIDDLE, MBT_MIDDLE);

    // Keyboard keys
    for (const auto& [key, state] : event.keys) {
        uint16_t val = static_cast<uint16_t>(key);
        if (val >= 400) continue; // mouse/gamepad já tratados

        int vk = inputKeyToVirtualKey(key);
        if (vk == 0) continue;

        CefKeyEvent cefEvent;
        cefEvent.windows_key_code = vk;
        cefEvent.modifiers = m_currentModifiers;

        if (state == INPUT_KEY_STATES::DOWN) {
            cefEvent.type = KEYEVENT_KEYDOWN;
            std::cout << "[CEF KEY] KEYDOWN vk=0x" << std::hex << cefEvent.windows_key_code
                      << " modifiers=0x" << cefEvent.modifiers << std::dec << std::endl;
            host->SendKeyEvent(cefEvent);

            // Não enviar CHAR quando Ctrl ou Alt estão ativos (ex: Ctrl+V = paste, não digitar "v")
            if (!(m_currentModifiers & EVENTFLAG_CONTROL_DOWN) && !(m_currentModifiers & EVENTFLAG_ALT_DOWN)) {
                char c = inputKeyToChar(key);
                if (c != 0) {
                    // Converter para minúscula se Shift não está ativo
                    if (!(m_currentModifiers & EVENTFLAG_SHIFT_DOWN) && c >= 'A' && c <= 'Z') {
                        c = c + ('a' - 'A');
                    }
                    cefEvent.type = KEYEVENT_CHAR;
                    cefEvent.character = c;
                    cefEvent.windows_key_code = c;
                    host->SendKeyEvent(cefEvent);
                }
            }
        
        }
        else if (state == INPUT_KEY_STATES::UP) {
            cefEvent.type = KEYEVENT_KEYUP;
            host->SendKeyEvent(cefEvent);
        }
    }
}

} // namespace lite
