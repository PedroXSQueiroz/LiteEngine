#pragma once

#include <editor/ObjectSelectorSystem.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>
#include <filament/scene/FilamentScene.h>

namespace lite {

// Picking sobre a FilamentScene: apenas fixa os parâmetros de template — toda
// a lógica (unprojection, AABB, Möller–Trumbore) é herdada da base agnóstica,
// que opera só sobre GLM e os contratos do core (nenhum recurso Filament).
class FilamentObjectSelectorSystem
    : public ObjectSelectorSystem<::FilamentScene, FilamentAsset3dTransform> {
public:
    ~FilamentObjectSelectorSystem() override = default;
};

} // namespace lite
