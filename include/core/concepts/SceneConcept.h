#pragma once

#include <core/concepts/TransformConcept.h>
#include <core/concepts/Asset3dConcept.h>
#include <core/concepts/Asset3dInstanceFactoryConcept.h>
#include <core/concepts/UIRendererConcept.h>

#include <concepts>

namespace lite{

    // Forward declaration: incluir Scene.h aqui fecharia um ciclo, já que
    // Scene.h inclui o umbrella EngineConcepts.h, que inclui este header.
    // As constraints têm de ser idênticas às de Scene.h.
    template<
        Asset3dConcept AssetType,
        TransformConcept TransformType,
        Asset3dInstanceFactoryConcept InstanceFactory,
        UIRendererConcept UIRenderer>
    class Scene;

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