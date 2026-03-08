#pragma once

#include <concepts>
#include <core/ui/UIRenderer.h>

namespace lite {

    template<typename T>
    concept UIRendererConcept = std::derived_from<T, UIRenderer<typename T::RendererType>>;

} // namespace lite
