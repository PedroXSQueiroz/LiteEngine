#pragma once

#include <iostream>

#include <core/view/View.h>
#include <filament/scene/FilamentScene.h>

#include <glm/glm.hpp>

#include <SDL.h>
#include <SDL_syswm.h>

namespace lite {

    class SDLFilamentView : public lite::View{

        public:
        
        SDLFilamentView(int width, int height):
        lite::View(),
        m_width(width),
        m_height(height)
        {

        };

        virtual bool Init() override{
            m_window = SDL_CreateWindow(
                "Lite",
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                m_width,
                m_height,
                SDL_WINDOW_ALLOW_HIGHDPI |
                SDL_WINDOW_SHOWN |
                SDL_WINDOW_RESIZABLE |
                SDL_WINDOW_FULLSCREEN_DESKTOP
            );

            if (!m_window) {
                std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
                return false;
            }

            return true;
        };

        void* getNativeWindow()
        {
            return getNativeWindowHandle(m_window);
        }

        virtual glm::vec2 getDimensions() override{
            int fbW = m_width, fbH = m_height;
            SDL_GetWindowSize(m_window, &fbW, &fbH);
            return glm::vec2(fbW, fbH);
        };        

        private:

        SDL_Window* m_window;
        int m_width;
        int m_height;

        static void* getNativeWindowHandle(SDL_Window* window) {
            SDL_SysWMinfo wmi;
            SDL_VERSION(&wmi.version);
            if (!SDL_GetWindowWMInfo(window, &wmi)) {
                return nullptr;
            }
            #if defined(_WIN32)
                return (void*) wmi.info.win.window;
            #elif defined(__APPLE__)
                return wmi.info.cocoa.window;
            #else
                return (void*) (uintptr_t) wmi.info.x11.window;
            #endif
        }

    };

}