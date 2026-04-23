#pragma once

namespace lite {

class SceneScopeSystem {
public:
    virtual ~SceneScopeSystem() = default;

    virtual void preRenderScene(float deltaTime) {}
    virtual void postRenderScene(float deltaTime) {}
};

} // namespace lite
