#include <concepts>
#include <glm/glm.hpp>

#include <core/data/assets/Asset3dTransform.h>

namespace lite {
    
    template<typename T>
    concept TargetTransformTypeConcept = std::derived_from<T, Asset3dTransform>;
    
    template<TargetTransformTypeConcept TransformType>
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