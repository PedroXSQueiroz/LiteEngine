#pragma once
#include <core/input/InputEvent.h>

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

    protected:

        int m_nextElementId = 1;


    };

}
