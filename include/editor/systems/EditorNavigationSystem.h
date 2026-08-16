#pragma once

#include <core/SceneScopeSystem.h>
#include <core/input/InputEnums.h>
#include <core/concepts/EngineConcepts.h>

#include <SDL.h>

#include <vector>
#include <memory>
#include <functional>

#include <glm/glm.hpp>

namespace lite{

    class EditorNavigationSystem: public SceneScopeSystem {
    
    public:
    bool    mov_front = false
        ,   mov_back = false
        ,   mov_right = false
        ,   mov_left = false
        ,   mov_up = false
        ,   mov_down = false
        ,   mov_active = false;

    bool left_button_down = false;
    
    const float radius              = 5.0f;
    const float VELOCITY_MOVEMENT   = radius * 50.f;
    const float VELOCITY_LOOK       = 0.003f;
    float horizontal_direction      = 0, vertical_direction = 0;

    glm::vec3 center                = glm::vec3(0, 0, 0);
    glm::vec3 offsetEye             = center + glm::vec3(0, radius * 0.3f, radius);
    glm::vec3 offsetCenter          = glm::normalize(center - offsetEye);
    glm::vec2 lastMousePosition     = glm::vec2(0, 0);

    glm::vec3 currentCamLocation    = glm::vec3(0, 0, 0);

    std::function<glm::vec3(glm::vec3, glm::vec3, glm::vec3)> onUpdateNavigationCallback;

    EditorNavigationSystem(std::function<glm::vec3(glm::vec3, glm::vec3, glm::vec3)> onUpdateNavigation):
    onUpdateNavigationCallback(onUpdateNavigation){        
    }

    void digestInputEvent(SDL_Event ev){
        switch (ev.type)
        {
        
        case SDL_MOUSEMOTION: {
            if(left_button_down)
            {
                horizontal_direction += ev.motion.xrel * VELOCITY_LOOK;
                vertical_direction   -= ev.motion.yrel * VELOCITY_LOOK;

                const float MAX_PITCH = 1.5f;
                if (vertical_direction >  MAX_PITCH) vertical_direction =  MAX_PITCH;
                if (vertical_direction < -MAX_PITCH) vertical_direction = -MAX_PITCH;

                float yaw   = horizontal_direction;
                float pitch = vertical_direction;

                offsetCenter.x = cos(pitch) * sin(yaw);
                offsetCenter.y = sin(pitch);
                offsetCenter.z = -cos(pitch) * cos(yaw);

            }

            // lastMousePosition = inputEvent.analogs[lite::INPUT_ANALOGS::MOUSE] =
            //     glm::vec2(ev.motion.x, ev.motion.y);

            lastMousePosition = glm::vec2(ev.motion.x, ev.motion.y);
            break;
        }
        case SDL_MOUSEBUTTONDOWN: {
            // lite::INPUT_KEYS btn = lite::INPUT_KEYS::MOUSE_LEFT;
            // if (ev.button.button == SDL_BUTTON_RIGHT)  btn = lite::INPUT_KEYS::MOUSE_RIGHT;
            // if (ev.button.button == SDL_BUTTON_MIDDLE) btn = lite::INPUT_KEYS::MOUSE_MIDDLE;
            // // inputEvent.keys[btn] = lite::INPUT_KEY_STATES::DOWN;
            // left_button_down = true;
            left_button_down = (ev.button.button == SDL_BUTTON_RIGHT);
            // inputEvent.analogs[lite::INPUT_ANALOGS::MOUSE] =
            //     glm::vec2(ev.button.x, ev.button.y);
            break;
        }
        case SDL_MOUSEBUTTONUP: {
            // lite::INPUT_KEYS btn = lite::INPUT_KEYS::MOUSE_LEFT;
            // if (ev.button.button == SDL_BUTTON_RIGHT)  btn = lite::INPUT_KEYS::MOUSE_RIGHT;
            // if (ev.button.button == SDL_BUTTON_MIDDLE) btn = lite::INPUT_KEYS::MOUSE_MIDDLE;
            // inputEvent.keys[btn] = lite::INPUT_KEY_STATES::UP;
            
            if(ev.button.button == SDL_BUTTON_RIGHT){
                left_button_down = false;
            }


            // inputEvent.analogs[lite::INPUT_ANALOGS::MOUSE] =
            //     glm::vec2(ev.button.x, ev.button.y);
            break;
        }
        // case SDL_MOUSEWHEEL:
        //     inputEvent.analogs[lite::INPUT_ANALOGS::MOUSE_WHEEL] =
        //         glm::vec2(ev.wheel.x * 120, ev.wheel.y * 120);
        //     break;

        case SDL_KEYDOWN: {
            
            if (ev.key.keysym.sym == SDLK_w)      mov_front = true;
            if (ev.key.keysym.sym == SDLK_s)      mov_back  = true;
            if (ev.key.keysym.sym == SDLK_d)      mov_right = true;
            if (ev.key.keysym.sym == SDLK_a)      mov_left  = true;
            if (ev.key.keysym.sym == SDLK_SPACE)  mov_up    = true;
            if (ev.key.keysym.sym == SDLK_LCTRL)  mov_down  = true;

            // if (ev.key.keysym.sym == SDLK_LSHIFT)  multiple_selection_active           = true;
            
            // INPUT_KEYS key = sdlKeyToInputKey(ev.key.keysym.sym);
            // if (key != INPUT_KEYS::KEY_UNKNOWN)
            //     inputEvent.keys[key] = lite::INPUT_KEY_STATES::DOWN;
            break;
        }
        case SDL_KEYUP: {

            if (ev.key.keysym.sym == SDLK_w)      mov_front = false;
            if (ev.key.keysym.sym == SDLK_s)      mov_back  = false;
            if (ev.key.keysym.sym == SDLK_d)      mov_right = false;
            if (ev.key.keysym.sym == SDLK_a)      mov_left  = false;
            if (ev.key.keysym.sym == SDLK_SPACE)  mov_up    = false;
            if (ev.key.keysym.sym == SDLK_LCTRL)  mov_down  = false;

            // if (ev.key.keysym.sym == SDLK_LSHIFT)  multiple_selection_active           = false;

            // INPUT_KEYS key = sdlKeyToInputKey(ev.key.keysym.sym);
            // if (key != INPUT_KEYS::KEY_UNKNOWN)
            //     inputEvent.keys[key] = lite::INPUT_KEY_STATES::UP;
            break;
        }
        // case SDL_WINDOWEVENT:
        //     if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        //         sceneRenderer.resize(ev.window.data1, ev.window.data2);
        //     }
        //     break;
        }
    }

    // Após instantiate() (assets criados neste frame já existem), antes do
    // prepareRender. Bom para: rastrear assets novos, lógica de simulação.
    virtual void onFrameBegin(float deltaTime) {
        
        if(left_button_down){
            glm::vec3 front = glm::normalize(offsetCenter);
            glm::vec3 upAbsolute(0, 1, 0);
            glm::vec3 right = glm::normalize(glm::cross(front, upAbsolute));
    
            glm::vec3 movement(0, 0, 0);
            if (mov_front) movement += front;
            if (mov_back)  movement -= front;
            if (mov_right) movement += right;
            if (mov_left)  movement -= right;
            if (mov_up)    movement += upAbsolute;
            if (mov_down)  movement -= upAbsolute;
    
            Uint64 now   = SDL_GetPerformanceCounter();
            static Uint64 prevTicks = SDL_GetPerformanceCounter();
            float deltaTime = (float)((now - prevTicks) / (double)SDL_GetPerformanceFrequency());
            prevTicks = now;
    
            if (glm::length(movement) > 0.001f) {
                offsetEye += glm::normalize(movement) * deltaTime * VELOCITY_MOVEMENT;
            }

            currentCamLocation = onUpdateNavigationCallback(center, offsetCenter, offsetEye);
        }
    }

    // Após finishRender() (deleções GPU já flushadas). Bom para: limpeza
    // ligada a assets deletados.
    virtual void onFrameEnd(float deltaTime) {

    }

    // ---- Dentro do frame GPU: pulados se prepareRender() retornar false ----

    // Logo após prepareRender() (beginFrame), antes dos preRenderScene.
    virtual void onRenderPrepared(float deltaTime) {}

    // Imediatamente antes de renderScene().
    virtual void preRenderScene(float deltaTime) {}

    // Imediatamente após renderScene().
    virtual void postRenderScene(float deltaTime) {}

    // Após a composição da UI (renderUI), antes de finishRender() (endFrame).
    virtual void onSceneRendered(float deltaTime) {}

    };

    


}