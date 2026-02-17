#pragma once

#include "include/cef_render_process_handler.h"
#include "include/wrapper/cef_message_router.h"

class CEF_UIRenderProcessHandler : public CefRenderProcessHandler {
public:
    void OnWebKitInitialized() override;

    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override;

    void OnContextReleased(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context) override;

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefProcessId source_process,
                                   CefRefPtr<CefProcessMessage> message) override;

private:
    CefRefPtr<CefMessageRouterRendererSide> m_messageRouter;
    IMPLEMENT_REFCOUNTING(CEF_UIRenderProcessHandler);
};
