#pragma once

namespace lite {

// Ponto de extensão único da Scene (padrão Interceptor / lifecycle hooks):
// sistemas registrados via Scene::addSystem são despachados em cada fase do
// frame, na ordem de registro. Todos os hooks têm corpo vazio default —
// implemente apenas as fases que o sistema precisa.
//
// THREADING: todos os hooks executam NA RENDER THREAD (dentro de Scene::update).
// Registrar/remover sistemas: antes de SceneRenderer::start() (main thread) ou
// via postCommand (render thread) — o vetor de sistemas não tem lock.
// Um hook NUNCA deve chamar Scene::get() de um id ainda na fila de criação:
// deadlock (instantiate() roda na mesma thread do hook).
class SceneScopeSystem {
public:
    virtual ~SceneScopeSystem() = default;

    // ---- Bordas do frame: rodam SEMPRE, mesmo se o frame GPU for pulado ----

    // Após instantiate() (assets criados neste frame já existem), antes do
    // prepareRender. Bom para: rastrear assets novos, lógica de simulação.
    virtual void onFrameBegin(float deltaTime) {}

    // Após finishRender() (deleções GPU já flushadas). Bom para: limpeza
    // ligada a assets deletados.
    virtual void onFrameEnd(float deltaTime) {}

    // ---- Dentro do frame GPU: pulados se prepareRender() retornar false ----

    // Logo após prepareRender() (beginFrame), antes dos preRenderScene.
    virtual void onRenderPrepared(float deltaTime) {}

    // Imediatamente antes de renderScene().
    virtual void preRenderScene(float deltaTime) {}

    // Imediatamente após renderScene().
    virtual void postRenderScene(float deltaTime) {}

    // Após a composição da UI (renderUI), antes de finishRender() (endFrame).
    virtual void onSceneRendered(float deltaTime) {}
};

} // namespace lite
