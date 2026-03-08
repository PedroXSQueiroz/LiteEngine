#pragma once

#include <core/concepts/EngineConcepts.h>
#include <core/scene/Scene.h>

#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>
#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>


class FilamentScene : public lite::Scene<
    lite::FilamentAsset3dInstance, 
    lite::FilamentAsset3dTransform,
    lite::FilamentInstanceFactory
>{
    public:
    FilamentScene(
        std::unique_ptr<lite::FilamentInstanceFactory> instanceFactory): Scene(std::move(instanceFactory)){};

    virtual bool renderScene() override {
        //FIXME: CHAMAR O RENDER DA FILAMENT, BEGIN FRAME E ETC AQUI.

        return true;
    };
};