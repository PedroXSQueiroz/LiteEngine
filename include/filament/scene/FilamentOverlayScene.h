#pragma once

#include <filament/scene/FilamentScene.h>

// Cena de OVERLAY: uma FilamentScene que NÃO é dona do frame e NÃO se desenha.
//
// A Scene do core é dona do ciclo do frame (prepareRender → beginFrame,
// finishRender → endFrame). Uma cena de overlay é conduzida por um
// SceneScopeSystem da cena principal, então abrir/fechar frame aqui seria um
// segundo beginFrame no mesmo swapchain. Por isso as duas fases são
// neutralizadas.
//
// O update() desta cena roda FORA do frame (no onFrameBegin do sistema), porque
// é ele que instancia os assets: FEngine::prepare() faz o commit das
// MaterialInstance uma vez por frame, dentro do beginFrame — instância criada
// depois disso chega ao render pass com descriptor set sem handle e o Filament
// aborta. Como o update() está fora do frame, renderScene() aqui é no-op: quem
// desenha a view do overlay é o sistema, no hook que roda dentro do frame.
//
// Consequência de tipo: esta cena não recebe swapChain (não tem o que fazer com
// ele). Também não recebe UIRenderer — a composição da UI é da cena principal.
//
// O que continua valendo do ciclo normal: instantiate() (cria os assets
// enfileirados, na render thread), os systems próprios e o flush das deleções
// da factory própria.
class FilamentOverlayScene : public FilamentScene {

public:
    FilamentOverlayScene(
        std::unique_ptr<lite::FilamentInstanceFactory> instanceFactory,
        filament::Renderer* filamentRenderer,
        filament::Scene* filamentScene,
        filament::View* view
    ) : FilamentScene(
            std::move(instanceFactory),
            nullptr,            // sem UI: Scene::update e renderUI já são null-guarded
            filamentRenderer,
            filamentScene,
            view,
            nullptr             // sem swapChain: esta cena não abre frame
        )
    {

    };

    // Sem beginFrame: o frame já está aberto pela cena principal.
    virtual bool prepareRender() override { return true; };

    // Sem desenho: este update() roda fora do frame. O render pass da view do
    // overlay é disparado pelo sistema dono da cena, dentro do frame.
    virtual bool renderScene() override { return true; };

    // Sem endFrame: quem fecha o frame é a cena principal. O flush das deleções
    // é da factory DESTA cena, então continua acontecendo aqui.
    virtual bool finishRender() override {
        this->m_asset3dFactory->flushDeletedFilament3dAssets();
        return true;
    };

};
