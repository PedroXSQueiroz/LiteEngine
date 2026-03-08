#pragma once

#include <glm/glm.hpp>

#include <core/concepts/TransformConcept.h>

namespace lite {

    template<TransformConcept TransformType>
    class TransformUtils {
    
    public:
    
        static TransformType build();
        
        static TransformType buildWithPosition(glm::vec3 position) {
            TransformType transf = build();
            return transf;
        }

        static TransformType buildWithRotation(glm::quat rotation) {
            TransformType transf = build();
            return transf;
        }

        static TransformType buildWithScale(glm::vec3 scale) {
            TransformType transf = build();
            return transf;
        }

        static TransformType build(glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
            TransformType transf = build();
            return transf;
        }
        
    };

};