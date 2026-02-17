#define NOMINMAX
#include <CEF/ui/CEF_UIRenderProcessHandler.h>

void CEF_UIRenderProcessHandler::OnWebKitInitialized() {
    CefMessageRouterConfig config;
    m_messageRouter = CefMessageRouterRendererSide::Create(config);
}

void CEF_UIRenderProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                                   CefRefPtr<CefFrame> frame,
                                                   CefRefPtr<CefV8Context> context)
{
    m_messageRouter->OnContextCreated(browser, frame, context);
}

void CEF_UIRenderProcessHandler::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                                    CefRefPtr<CefFrame> frame,
                                                    CefRefPtr<CefV8Context> context)
{
    m_messageRouter->OnContextReleased(browser, frame, context);
}

bool CEF_UIRenderProcessHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                                           CefRefPtr<CefFrame> frame,
                                                           CefProcessId source_process,
                                                           CefRefPtr<CefProcessMessage> message)
{
    return m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message);
}
