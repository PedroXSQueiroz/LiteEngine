#pragma once

#include <concepts>
#include <core/data/assets/Asset3dTransform.h>

namespace lite {

    template<typename T>
    concept TransformConcept = std::derived_from<T, Asset3dTransform>;

} // namespace lite
