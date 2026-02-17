#pragma once

#include <map>

#include <core/input/InputEvent.h>
#include <core/ui/elements/UIElementHandler.h>

namespace lite{
    
    template<typename R>
    class UIRenderer{
    
    public:

        using RendererType = R;

        UIRenderer() {};

        virtual bool start() = 0;

        virtual bool stop() = 0;

        virtual void update() = 0;

        virtual void render(R* renderer) = 0;

        virtual void sendInputEvent(const InputEvent& event) {}

        int nextElementId() { return m_nextElementId++; }

        void registerElement(int id, UIElementHandler handler) {
            this->m_uiElements.emplace(id, handler);
        }

        UIElementHandler getElement(int id) { return this->m_uiElements.at(id); }

    protected:

        int m_nextElementId = 1;

        std::map<int, UIElementHandler> m_uiElements;

    };

}
