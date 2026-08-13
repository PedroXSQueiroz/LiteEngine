#pragma once

#include <core/data/assets/IO/SceneDTOMapper.h>

#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>

namespace lite{

    template<typname UIRenderer>
    class FilamentSceneDTOMapper : public SceneDTOMapper<
        FilamentAsset3dInstance,
        FilamentAsset3dTransform,
        FilamentInstanceFactory,
        UIRenderer
    >
    {

    }

}