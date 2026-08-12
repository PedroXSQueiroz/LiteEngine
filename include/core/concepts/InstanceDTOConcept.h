#pragma once

#include <concepts>
#include <core/data/DTOs/Asset3dInstanceDTO.h>

namespace lite {

    // Aceita o próprio Asset3dInstanceDTO (nó sem dados extras) e qualquer
    // filha dele — std::derived_from<T, T> é verdadeiro.
    template<typename D>
    concept InstanceDTOConcept = std::derived_from<D, Asset3dInstanceDTO>;

} // namespace lite
