#pragma once

#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <filament/SwapChain.h>

#include <core/concepts/EngineConcepts.h>
#include <core/scene/Scene.h>

#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>
#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>
#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>

using namespace filament;

class FilamentScene : public lite::Scene<
    lite::FilamentAsset3dInstance, 
    lite::FilamentAsset3dTransform,
    lite::FilamentInstanceFactory,
    lite::CEF_Filament_UIRendererThreaded
>{

public:
    FilamentScene(
        std::unique_ptr<lite::FilamentInstanceFactory> instanceFactory,
        std::unique_ptr<lite::CEF_Filament_UIRendererThreaded> uiRenderer,
        filament::Renderer* filamentRenderer,
        filament::Scene* filamentScene,
        filament::View* view,
        filament::SwapChain* swapChain
    ): Scene(
        std::move(instanceFactory),
        std::move(uiRenderer)
    ),  m_filamentRenderer(filamentRenderer)
    ,   m_filamentScene(filamentScene)
    ,   m_filamentView(view)
    ,   m_swapChain(swapChain)
    {
       
    };

    virtual bool prepareRender() override {
        return this->m_filamentRenderer->beginFrame(this->m_swapChain);
    };

    virtual bool renderScene() override {
        this->m_filamentRenderer->render(this->m_filamentView);
        return true;
    };

    virtual void renderUI() override {
        auto* uiRenderer = this->getCurrentUI();
        if (uiRenderer) {
            uiRenderer->render(this->m_filamentRenderer);
        }
    };

    virtual bool finishRender() override {
        this->m_filamentRenderer->endFrame();
        this->m_asset3dFactory->flushDeletedFilament3dAssets();
        
        return true;
    };
    
    filament::Renderer* getFilamentRenderer() { return this->m_filamentRenderer; };

    filament::Scene* getFilamentScene() { return this->m_filamentScene; };

    filament::View* getFilamentView() { return this->m_filamentView; };

private:
    
    filament::Renderer* m_filamentRenderer;
    filament::Scene* m_filamentScene;
    filament::View* m_filamentView;
    filament::SwapChain* m_swapChain;

};