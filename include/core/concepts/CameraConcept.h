#pragma once

#include <concepts>
#include <core/concepts/TransformConcept.h>

namespace lite{

    // Forward declaration: incluir CameraAsset3dInstance.h aqui fecharia um ciclo,
    // já que aquele header inclui o umbrella EngineConcepts.h, que inclui este.
    // As constraints têm de ser idênticas às de CameraAsset3dInstance.h.
    template<TransformConcept Transform>
    class CameraAsset3dInstance;

    template<typename C>
    concept CameraConcept = 
        requires { typename C::TransformType; } && 
        std::derived_from<C, CameraAsset3dInstance<typename C::TransformType>>;

}