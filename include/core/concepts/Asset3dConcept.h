#pragma once

#include <concepts>
#include <core/data/assets/Asset3dInstance.h>

namespace lite {

    template<typename A>
    concept Asset3dConcept =
        requires { typename A::TransformType; } &&
        std::derived_from<A, Asset3dInstance<typename A::TransformType>>;

} // namespace lite
