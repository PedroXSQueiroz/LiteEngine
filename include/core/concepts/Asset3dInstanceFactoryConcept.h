#pragma once

#include <concepts>
#include <core/assets/instanceFactory/Asset3dInstanceFactory.h>
#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/Asset3dTransform.h>

namespace lite {

    template<typename ISF>
    concept Asset3dInstanceFactoryConcept =
        requires { typename ISF::AssetType; typename ISF::TransformType; } &&
        std::derived_from<
            ISF, Asset3dInstanceFactory<
                typename ISF::AssetType,
                typename ISF::TransformType
            >
        >;

}