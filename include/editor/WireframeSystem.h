#pragma once

#include <core/concepts/MeshAsset3dConcept.h>

#include <glm/glm.hpp>
#include <unordered_set>
#include <cstdint>

namespace lite {

// Base class for wireframe overlay system - renderer agnostic
template <MeshAsset3dConcept MeshType>
class WireframeSystem {
public:
    virtual ~WireframeSystem() = default;

    // Initialize with viewport dimensions
    virtual void initialize(uint32_t width, uint32_t height) = 0;

    // Update when viewport resizes
    virtual void resize(uint32_t width, uint32_t height) = 0;

    // Configuration
    virtual void setWireframeColor(const glm::vec4& color) = 0;
    virtual void setWireframeWidth(float width) = 0;

    // Wireframe mesh management
    virtual void addWireframeMesh(MeshType* mesh) = 0;
    virtual void removeWireframeMesh(MeshType* mesh) = 0;
    virtual void clearWireframeMeshes() = 0;

    // Called each frame to sync transforms
    virtual void beginFrame() = 0;

    // Getters
    const glm::vec4& getWireframeColor() const { return m_wireframeColor; }
    float getWireframeWidth() const { return m_wireframeWidth; }
    bool hasWireframeMeshes() const { return !m_wireframeMeshes.empty(); }

protected:
    glm::vec4 m_wireframeColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.8f); // White
    float m_wireframeWidth = 1.5f;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::unordered_set<MeshType*> m_wireframeMeshes;
};

} // namespace lite
