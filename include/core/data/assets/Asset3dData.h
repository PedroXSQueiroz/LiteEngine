#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lite {

// Base class for 3D scene nodes
// This IS the node - no separate SceneNode concept
class Asset3dData {
public:
    virtual ~Asset3dData() = default;

    // Node identification
    std::string name;

    // Transform (relative to parent)
    glm::mat4 localTransform = glm::mat4(1.0f);

    // Hierarchy
    Asset3dData* parent = nullptr;  // Raw pointer (does not own)
    std::vector<std::unique_ptr<Asset3dData>> children;

    // Calculate world transform recursively
    glm::mat4 getWorldTransform() const;

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

    virtual std::unique_ptr<Asset3dData> clone() const {
        auto copy = std::make_unique<Asset3dData>();
        copy->name = name;
        copy->localTransform = localTransform;
        for (const auto& child : children) {
            auto childClone = child->clone();
            childClone->parent = copy.get();
            copy->children.push_back(std::move(childClone));
        }
        return copy;
    }
};

} // namespace lite
