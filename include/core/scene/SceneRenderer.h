#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <queue>
#include <functional>
#include <memory>
#include <string>
#include <chrono>

#include <glm/glm.hpp>

#include <core/view/View.h>

namespace lite {

// Facade abstrato do renderer de cena: possui a render thread e orquestra o
// ciclo de vida dela (Template Method):
//
//   renderThreadMain():
//       setup()        → fase virtual: engine, cena, recursos (bool: falha → cleanup)
//       waitStart()    → spin-wait por start(), processando a fila de comandos
//       renderLoop()   → esqueleto concreto do loop; por frame chama renderFrame(dt)
//       cleanup()      → fase virtual: teardown completo (roda SEMPRE)
//
// Implementações concretas (ex.: FilamentSceneRenderer) fornecem
// setup()/renderFrame()/cleanup() e populam m_scene durante o setup.
//
// THREADING:
// - Toda fase virtual (setup/renderFrame/cleanup) executa NA render thread.
// - O construtor desta base NÃO inicia a thread: a thread chamaria virtuais de
//   um objeto ainda em construção (vtable incompleta → pure virtual call).
//   A classe FILHA deve chamar launchRenderThread() como ÚLTIMA instrução do
//   próprio construtor.
// - A classe FILHA deve chamar stop() no próprio destrutor: o join precisa
//   completar antes da parte derivada do objeto ser destruída. O stop() do
//   destrutor desta base é apenas cinto de segurança.
template<typename SceneType>
class SceneRenderer {
public:
    virtual ~SceneRenderer() { stop(); }

    // Block until render thread has finished setup (or failed it)
    void waitReady() { m_readyFuture.wait(); }

    // Start the render loop (call after waitReady + setup of callbacks/systems)
    void start() { m_started.store(true); }

    // Stop the render loop and join the render thread (idempotente)
    void stop() {
        m_running.store(false);
        m_started.store(true);  // unblock spin-wait if start() was never called
        if (m_renderThread.joinable()) {
            m_renderThread.join();
        }
    }

    // Cena concreta (nula se setup() falhou)
    SceneType* getScene() { return m_scene.get(); }

    // Thread-safe camera state update (consumida pela render thread via takePendingCamera)
    void setCameraState(const glm::vec3& eye, const glm::vec3& target) {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        m_pendingCamera.eye = eye;
        m_pendingCamera.target = target;
        m_pendingCamera.dirty = true;
    }

    // Post an arbitrary command to run on the render thread
    void postCommand(std::function<void()> command) {
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        m_commands.push(std::move(command));
    }

    // Comandos de conteúdo — assinaturas agnósticas (GLM/string); implementações
    // concretas devem postá-los à fila da render thread (postCommand)
    virtual void setIBL(const std::string& path, float intensity = 30000.0f) = 0;
    virtual void addDirectionalLight(const glm::vec3& color, float intensity,
                                     const glm::vec3& direction, bool castShadows = false) = 0;
    virtual void resize(int width, int height) = 0;

protected:
    SceneRenderer(
            lite::View* view
            // void* nativeWindowHandle
        ,   int width
        ,   int height)
        :   m_view(view)
            // m_nativeWindowHandle(nativeWindowHandle)
        ,   m_width(width)
        ,   m_height(height)
        ,   m_readyFuture(m_readyPromise.get_future()) {}

    // THREADING: chamar como ÚLTIMA instrução do construtor da classe filha
    void launchRenderThread() {
        m_renderThread = std::thread(&SceneRenderer::renderThreadMain, this);
    }

    // --- Fases virtuais: executam na render thread ---
    virtual bool setup() = 0;             // false → pula direto para cleanup()
    virtual void renderFrame(float dt) = 0;
    virtual void cleanup() = 0;

    // Consome o estado pendente de câmera (chamar dentro de renderFrame).
    // Retorna false se não houve atualização desde o último consumo.
    bool takePendingCamera(glm::vec3& eye, glm::vec3& target) {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        if (!m_pendingCamera.dirty) return false;
        eye = m_pendingCamera.eye;
        target = m_pendingCamera.target;
        m_pendingCamera.dirty = false;
        return true;
    }

    // void* m_nativeWindowHandle;
    int m_width;
    int m_height;

    // Populada pela filha em setup(); destruída pela filha em cleanup()/reset
    std::unique_ptr<SceneType> m_scene;

    //TODO: MAYBE SHOULD BE UNITQUE PTR
    lite::View* m_view;

private:
    void renderThreadMain() {
        if (setup()) {
            m_running.store(true);

            // Signal main thread: setup complete
            m_readyPromise.set_value();

            waitStart();
            renderLoop();

            // Drena comandos de teardown postados durante o shutdown (ex.:
            // destruir sistemas com recursos GPU) — ainda na render thread,
            // antes do cleanup. Garante que "postCommand + stop()" na main
            // execute o comando mesmo se o loop saiu antes de processá-lo.
            processCommands();
        } else {
            // Destrava waitReady() mesmo em falha (getScene() retornará nulo)
            m_readyPromise.set_value();
        }

        cleanup();
    }

    void waitStart() {
        // Spin-wait for start() — process setup commands (IBL, lights, etc.) while waiting
        while (!m_started.load() && m_running.load()) {
            processCommands();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Process any remaining setup commands before the first frame
        processCommands();
    }

    void renderLoop() {
        auto prevTime = std::chrono::high_resolution_clock::now();

        while (m_running.load()) {
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - prevTime).count();
            prevTime = now;

            processCommands();

            renderFrame(dt);

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void processCommands() {
        std::queue<std::function<void()>> batch;
        {
            std::lock_guard<std::mutex> lock(m_commandsMutex);
            std::swap(batch, m_commands);
        }
        while (!batch.empty()) {
            batch.front()();
            batch.pop();
        }
    }

    std::thread m_renderThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_started{false};
    std::promise<void> m_readyPromise;
    std::future<void> m_readyFuture;

    struct CameraState {
        glm::vec3 eye{0.0f, 0.0f, 5.0f};
        glm::vec3 target{0.0f, 0.0f, 0.0f};
        bool dirty{false};
    };
    CameraState m_pendingCamera;
    std::mutex m_cameraMutex;

    std::queue<std::function<void()>> m_commands;
    std::mutex m_commandsMutex;
};

} // namespace lite
