#pragma once

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

    };

}
