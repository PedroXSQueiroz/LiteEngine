#pragma once

#include <core/SceneScopeSystem.h>
#include <core/scene/Scene.h>
#include <core/ui/elements/UIElements.h>
#include <core/concepts/EngineConcepts.h>

#include <memory>
#include <vector>

namespace lite{
    
    template<SceneConcept SceneType>
    class SceneConfigurer{

        public:
        
        virtual SceneType* configure(SceneType* scene) = 0;

    };

}