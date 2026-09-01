#pragma once

#include <core/concepts/EngineConcepts.h>
// #include <core/scene/SceneRenderer.h>

#include <glm/glm.hpp>

namespace lite{

    // template<SceneConcept SceneType>
    class View{

    public:
    
    // Init() NÃO é chamada aqui: durante a construção da base o tipo dinâmico
    // ainda é View, então a chamada seria desvirtualizada para View::Init (pura)
    // e nunca chegaria ao override. Quem inicializa é SceneRenderer::renderThreadMain,
    // via m_view->Init(), com o objeto já completo.
    View(){}
    
    virtual glm::vec2 getDimensions() = 0;
    
    virtual bool Init() = 0;
    
    };

}