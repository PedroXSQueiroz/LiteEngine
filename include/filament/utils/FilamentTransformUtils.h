#include <core/utils/TransformUtils.h>

#include <filament/data/assets/FilamentAsset3dTransform.h>

namespace lite {
    
    template<>
    FilamentAsset3dTransform TransformUtils<FilamentAsset3dTransform>::build();
}