#pragma once

#include "include/cef_render_process_handler.h"
#include "include/wrapper/cef_message_router.h"

class CEF_UIRenderProcessHandler : public CefRenderProcessHandler {
public:
    void OnWebKitInitialized() override {
        CefMessageRouterConfig config;
        m_messageRouter = CefMessageRouterRendererSide::Create(config);
    }

    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override
    {
        m_messageRouter->OnContextCreated(browser, frame, context);
    }

    void OnContextReleased(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context) override
    {
        m_messageRouter->OnContextReleased(browser, frame, context);
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefProcessId source_process,
                                   CefRefPtr<CefProcessMessage> message) override
    {
        return m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message);
    }

private:
    CefRefPtr<CefMessageRouterRendererSide> m_messageRouter;
    IMPLEMENT_REFCOUNTING(CEF_UIRenderProcessHandler);
};
