#pragma once

#include <concepts>
#include <core/data/assets/MeshAsset3dInstance.h>

namespace lite {

    template<typename M>
    concept MeshAsset3dConcept =
        requires { typename M::TransformType; } &&
        std::derived_from<M, MeshAsset3dInstance<typename M::TransformType>>;

} // namespace lite
