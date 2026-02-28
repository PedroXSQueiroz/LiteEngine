#include <filament/utils/FilamentUtils.h>
#include <filament/utils/FilamentTransformUtils.h>

#include <utils/EntityManager.h>
#include <filament/TransformManager.h>

using namespace utils;
using namespace filament;

template<>
lite::FilamentAsset3dTransform lite::TransformUtils<lite::FilamentAsset3dTransform>::build() {
    // implementação específica da Filament
    return FilamentAsset3dTransform(
        FilamentUtils::getEngine()->getTransformManager()
    );
}