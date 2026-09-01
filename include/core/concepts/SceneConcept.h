#pragma once

#include <core/scene/Scene.h>

namespace lite{

    template<typename S>
    concept SceneConcept = 
    requires {  typename S::SceneAsset3d;
                typename S::SceneTransform;
                typename S::SceneAsset3dFactory;
                typename S::SceneUIRenderer;
            } &&
    std::derived_from<S, lite::Scene<
        typename S::SceneAsset3d,
        typename S::SceneTransform,
        typename S::SceneAsset3dFactory,
        typename S::SceneUIRenderer
    >>;

}