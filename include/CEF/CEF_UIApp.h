#pragma once

#include "include/cef_app.h"
#include "include/cef_command_line.h"

class CEF_UIApp : public CefApp {
public:
    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override
    {
        command_line->AppendSwitch("allow-file-access-from-files");
        command_line->AppendSwitch("allow-universal-access-from-files");
    }

    IMPLEMENT_REFCOUNTING(CEF_UIApp);
};
