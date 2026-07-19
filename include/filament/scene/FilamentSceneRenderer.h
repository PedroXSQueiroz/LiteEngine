#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <queue>
#include <functional>
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <utils/Entity.h>

#include <filament/scene/FilamentScene.h>
#include <filament/data/assets/FilamentCameraAsset3dInstance.h>
#include <filament/lightning/FilamentIBL.h>

namespace filament {
    class Engine;
    class SwapChain;
}

class FilamentSceneRenderer {
public:
    FilamentSceneRenderer(void* nativeWindowHandle, int width, int height);
    ~FilamentSceneRenderer();

    // Block until render thread has finished setup (engine, scene, camera created)
    void waitReady();

    // Start the render loop (call after waitReady + setup of callbacks/systems)
    void start();

    // Stop the render loop and join the render thread
    void stop();

    FilamentScene* getScene();

    // Thread-safe camera state update (applied by render thread each frame)
    void setCameraState(const glm::vec3& eye, const glm::vec3& target);

    // Post IBL load to render thread command queue
    void setIBL(const std::string& path, float intensity = 30000.0f);

    // Post directional light creation to render thread command queue
    void addDirectionalLight(const glm::vec3& color, float intensity,
                             const glm::vec3& direction, bool castShadows = false);

    // Post viewport + projection update to render thread command queue
    void resize(int width, int height);

    // Post an arbitrary command to run on the render thread
    void postCommand(std::function<void()> command);

private:
    void renderLoop();
    void processCommands();

    void* m_nativeWindowHandle;
    int m_width;
    int m_height;

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

    // Owned and accessed exclusively by the render thread after construction
    filament::Engine* m_engine{nullptr};
    filament::SwapChain* m_swapChain{nullptr};
    std::unique_ptr<FilamentScene> m_scene;
    std::unique_ptr<lite::FilamentCameraAsset3dInstance> m_camera;
    std::unique_ptr<lite::FilamentIBL> m_ibl;
    utils::Entity m_lightEntity;
};
