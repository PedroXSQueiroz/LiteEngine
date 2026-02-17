#pragma once

// #include <core/ui/UIRenderer.h>
#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>
#include <core/ui/UIInstance.h>

#include <filament/Renderer.h>


namespace lite{

    class CEF_Filament_UIInstance: public UIInstance<CEF_Filament_UIRendererThreaded> {

    public:
        explicit CEF_Filament_UIInstance(CEF_Filament_UIRendererThreaded* uiRenderer)
            : UIInstance<CEF_Filament_UIRendererThreaded>(uiRenderer) {}

    protected:

        virtual UIPanelElement<CEF_Filament_UIRendererThreaded>* createRoot() override {
            return new CEF_UIPanelElement(this->m_uiRenderer);
        };

    };

}
