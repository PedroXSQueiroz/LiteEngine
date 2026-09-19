#include <core/concepts/EngineConcepts.h>

namespace lite{
    
    //A IDEIA É RETORNAR A CENA BÁSICA COM A IMPLEMENTAÇÃO CONCRETA
    //
    template<SceneConcept SceneType>
    class SceneFactory
    {
        public:
        virtual SceneType* build() = 0;
    
    };

} 