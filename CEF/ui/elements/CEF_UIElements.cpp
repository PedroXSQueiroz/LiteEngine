#pragma once

#include <core/ui/UIRenderer.h>
#include <CEF/ui/elements/CEF_UIElements.h>

namespace lite {
    //-----------------------------------------------------------------------------------
    //PANEL
    //-----------------------------------------------------------------------------------
    int CEF_UIPanelElement::drawContainer(
        int parentId,
        int line,
        int column,
        int lineSpan,
        int columnSpan
    ){
        return UIElement::EMPTY_ELEMENT_ID;
    }

    bool CEF_UIPanelElement::isFoccused() {
        return false;
    }
    

    //-----------------------------------------------------------------------------------
    //TEXT
    //-----------------------------------------------------------------------------------
    bool CEF_UITextElement::isFoccused() {
        return false;
    }
    
    int CEF_UITextElement::draw(
            int parentId, 
            int line, 
            int column, 
            int lineSpan, 
            int columnSpan
    ) {
        
        this->m_uiRenderer->executeJavaScriptThrottled("createTextComponent('')");
        
        return 0;
    }

    bool CEF_UITextElement::setText(std::string text) {
        
        this->m_uiRenderer->executeJavaScriptThrottled(
            "getUIElement(" + std::to_string(this->m_currentId) + ").setText('" + text + "');",
            30  // ~30fps para UI, suficiente para feedback visual
        );
        
        return true;
    }

    std::string CEF_UITextElement::getText() {
        return "";
    }

} // namespace lite