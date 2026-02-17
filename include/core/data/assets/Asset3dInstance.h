#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <core/data/assets/Asset3dTransform.h>

namespace lite {

// Base class for instantiated 3D asset nodes (GPU resources)
// This IS the node - mirrors Asset3dData hierarchy
class Asset3dInstance {
public:
    virtual ~Asset3dInstance() = default;

    // Node identification
    std::string name;

    // Transform access (implementation provides concrete transform)
    Asset3dTransform* getTransform() { return m_transform.get(); }
    const Asset3dTransform* getTransform() const { return m_transform.get(); }

    // Convenience methods that delegate to transform
    glm::mat4 getLocalMatrix() const;
    glm::mat4 getWorldMatrix() const;
    void setLocalMatrix(const glm::mat4& matrix);

    // Hierarchy
    Asset3dInstance* parent = nullptr;  // Raw pointer (does not own)
    std::vector<std::unique_ptr<Asset3dInstance>> children;

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
    std::unique_ptr<Asset3dTransform> m_transform;
    bool m_visible = true;
};

} // namespace lite
