#pragma once

#include <core/ui/elements/UIElements.h>
#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>
#include <filament/Renderer.h>

using namespace filament;

namespace lite {

    class CEF_UIPanelElement: public UIPanelElement<CEF_Filament_UIRendererThreaded>{

    public:
        CEF_UIPanelElement(CEF_Filament_UIRendererThreaded* renderer): UIPanelElement(renderer) {};

        virtual int drawContainer(
            int parentId = EMPTY_ELEMENT_ID, 
            int line = 0, 
            int column = 0, 
            int lineSpan = 0, 
            int columnSpan = 0
        ) override;

        virtual bool isFoccused() override;

    };
    
    class CEF_UITextElement : public UITextElement<CEF_Filament_UIRendererThreaded> {
    public:
        
        CEF_UITextElement(CEF_Filament_UIRendererThreaded* renderer): UITextElement(renderer) {};

        virtual bool isFoccused() override;

        virtual int draw(
            int parentId = EMPTY_ELEMENT_ID, 
            int line = 0, 
            int column = 0, 
            int lineSpan = 0, 
            int columnSpan = 0
        ) override;

        virtual bool setText(std::string text) override;

        virtual std::string getText() override;
    };

} // namespace lite