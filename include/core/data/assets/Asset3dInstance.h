#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>

namespace lite {

// Base class for instantiated 3D asset nodes (GPU resources)
// This IS the node - mirrors Asset3dData hierarchy
class Asset3dInstance {
public:
    virtual ~Asset3dInstance() = default;

    // Node identification
    std::string name;

    // Transform (relative to parent)
    glm::mat4 localTransform = glm::mat4(1.0f);

    // Hierarchy
    Asset3dInstance* parent = nullptr;  // Raw pointer (does not own)
    std::vector<std::unique_ptr<Asset3dInstance>> children;

    // Calculate world transform recursively
    glm::mat4 getWorldTransform() const {
        if (!parent) {
            return localTransform;
        }
        return parent->getWorldTransform() * localTransform;
    }

    // Add child node with automatic parent assignment
    template<typename T, typename... Args>
    T* addChild(Args&&... args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        child->parent = this;
        T* ptr = child.get();
        children.push_back(std::move(child));
        return ptr;
    }

    // Type identification
    virtual bool isMesh() const { return false; }

    // Visibility control
    virtual void setVisible(bool visible) { m_visible = visible; }
    virtual bool isVisible() const { return m_visible; }

protected:
    bool m_visible = true;
};

} // namespace lite
