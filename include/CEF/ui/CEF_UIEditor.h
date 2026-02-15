#pragma once

#include <editor/UI/UIEditor.h>
#include <core/ui/CEFUIRendererThreaded.h>

namespace lite{
    
    class CEF_UIEditor: public UIEditor<CEFUIRendererThreaded>{
        
    public:
        CEF_UIEditor(CEFUIRendererThreaded* uiRenderer): UIEditor(uiRenderer){}

    };

}
