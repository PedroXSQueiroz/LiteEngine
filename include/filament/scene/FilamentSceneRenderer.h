#pragma once

#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <utils/Entity.h>

#include <core/scene/SceneRenderer.h>
#include <core/view/View.h>
#include <filament/scene/FilamentScene.h>
#include <filament/data/assets/FilamentCameraAsset3dInstance.h>
#include <filament/lightning/FilamentIBL.h>

namespace filament {
    class Engine;
    class SwapChain;
}

namespace lite {

// Implementação Filament do facade lite::SceneRenderer.
// A base fornece a render thread, a fila de comandos, o handshake waitReady/start
// e o esqueleto do loop; esta classe fornece as fases setup/renderFrame/cleanup
// (todas executando na render thread — thread affinity do filament::Engine).
class FilamentSceneRenderer : public SceneRenderer<FilamentScene> {
public:
    FilamentSceneRenderer(View* view, int width, int height);
    ~FilamentSceneRenderer() override;

    // Comandos de conteúdo (postados à fila da render thread)
    void setIBL(const std::string& path, float intensity) override;
    void addDirectionalLight(const glm::vec3& color, float intensity,
                             const glm::vec3& direction, bool castShadows) override;
    void resize(int width, int height) override;

    FilamentCameraAsset3dInstance* getCurrentCamera(){
        return this->m_camera.get();
    }

protected:
    // Fases do ciclo de vida — executam na render thread (ver SceneRenderer.h)
    bool setup() override;              // engine, swapchain, FilamentScene (+ UI GPU), câmera
    void renderFrame(float dt) override; // câmera pendente + m_scene->update(dt)
    void cleanup() override;            // teardown completo; tolerante a setup parcial

private:
    // Owned and accessed exclusively by the render thread after construction
    filament::Engine* m_engine{nullptr};
    filament::SwapChain* m_swapChain{nullptr};
    std::unique_ptr<FilamentCameraAsset3dInstance> m_camera;
    std::unique_ptr<FilamentIBL> m_ibl;
    utils::Entity m_lightEntity;
    // void* m_nativeWindowHandle;
};

} // namespace lite
