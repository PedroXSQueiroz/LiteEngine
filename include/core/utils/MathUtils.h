#pragma once

#include <glm/glm.hpp>

#include <core/concepts/EngineConcepts.h>
#include <core/data/assets/CameraAsset3dInstance.h>

namespace lite{
    class MathUtils{

    public:
    
    template<TransformConcept TransformType>
    static glm::vec3 calcScreenPixelRay(
        CameraAsset3dInstance<TransformType>* camera,
        const glm::vec2& pixel, 
        const glm::vec2& viewportSize,
        float length
    ){
        if (!camera || viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
            return glm::vec3(0.0f);
        }

        // viewport → NDC: y da tela cresce para baixo, NDC cresce para cima
        const float x = (2.0f * pixel.x) / viewportSize.x - 1.0f;
        const float y = 1.0f - (2.0f * pixel.y) / viewportSize.y;

        const glm::mat4 world = camera->getWorldMatrix();
        const glm::mat4 view = glm::inverse(world); // localização do nó, não a view do renderer
        const glm::mat4 invVP = glm::inverse(camera->getProjectionMatrix() * view);

        // clip → mundo: dois depths distintos do mesmo pixel caem sobre a
        // mesma reta no mundo, independente da convenção de z da projeção
        // (NO [-1,1] ou ZO [0,1]) — dispensa acertar o valor do near plane
        glm::vec4 p0 = invVP * glm::vec4(x, y, 0.0f, 1.0f);
        glm::vec4 p1 = invVP * glm::vec4(x, y, 0.5f, 1.0f);
        constexpr float eps = 1e-8f;
        if (std::abs(p0.w) < eps || std::abs(p1.w) < eps) return glm::vec3(0.0f);

        const glm::vec3 a = glm::vec3(p0) / p0.w;
        const glm::vec3 b = glm::vec3(p1) / p1.w;
        glm::vec3 dir = b - a;
        if (glm::dot(dir, dir) < eps) return glm::vec3(0.0f);
        dir = glm::normalize(dir);

        // orienta para frente da câmera (o par de depths pode sair invertido
        // conforme a convenção de projeção)
        const glm::vec3 forward = -glm::normalize(glm::vec3(world[2]));
        if (glm::dot(dir, forward) < 0.0f) dir = -dir;

        return dir * length;
    }

    }; 

}