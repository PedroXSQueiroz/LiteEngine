#pragma once

#include <concepts>
#include <core/data/DTOs/SceneDTO.h>

namespace lite {

    // Aceita o próprio SceneDTO e qualquer filha dele — std::derived_from<T, T>
    // é verdadeiro.
    template<typename D>
    concept SceneDTOConcept = std::derived_from<D, SceneDTO>;

} // namespace lite
