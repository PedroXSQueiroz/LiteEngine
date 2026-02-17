#define NOMINMAX
#include <CEF/CEF_UIApp.h>

void CEF_UIApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line)
{
    command_line->AppendSwitch("allow-file-access-from-files");
    command_line->AppendSwitch("allow-universal-access-from-files");
}

CefRefPtr<CefRenderProcessHandler> CEF_UIApp::GetRenderProcessHandler() {
    if (!m_renderHandler)
        m_renderHandler = new CEF_UIRenderProcessHandler();
    return m_renderHandler;
}
