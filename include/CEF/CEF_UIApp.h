#pragma once

#include "include/cef_app.h"
#include "include/cef_command_line.h"

#include "CEF/ui/CEF_UIRenderProcessHandler.h"

class CEF_UIApp : public CefApp {
public:
    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override
    {
        command_line->AppendSwitch("allow-file-access-from-files");
        command_line->AppendSwitch("allow-universal-access-from-files");
    }

    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        if (!m_renderHandler)
            m_renderHandler = new CEF_UIRenderProcessHandler();
        return m_renderHandler;
    }

private:
    CefRefPtr<CEF_UIRenderProcessHandler> m_renderHandler;
    IMPLEMENT_REFCOUNTING(CEF_UIApp);
};
