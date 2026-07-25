#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <core/concepts/TransformConcept.h>

namespace lite {

// Base class for instantiated 3D asset nodes (GPU resources)
// This IS the node - mirrors Asset3dData hierarchy
template<TransformConcept Transform>
class Asset3dInstance {
public:

    using TransformType = Transform;

    Asset3dInstance(){};

    Asset3dInstance(std::unique_ptr<Transform> transform):m_transform(std::move(transform)){};

    virtual ~Asset3dInstance() = default;

    // Node identification
    std::string name;

    // Id no espaço de numeração da Scene (o mesmo usado como chave de
    // m_3dInstances para a raiz). -1 = sem id: nó fora de cena (ex.: câmera do
    // renderer) ou filho de asset criado sem deepIds.
    // A atribuição é responsabilidade exclusiva da Scene (create/instantiate).
    int getId() const { return m_id; }
    void setId(int id) { m_id = id; }

    // Transform access (implementation provides concrete transform)
    Transform* getTransform() { return m_transform.get(); }
    const Transform* getTransform() const { return m_transform.get(); }

    // Convenience methods that delegate to transform
    glm::mat4 getLocalMatrix() const {
        return m_transform ? m_transform->getLocalMatrix() : glm::mat4(1.0f);
    }
    glm::mat4 getWorldMatrix() const {
        return m_transform ? m_transform->getWorldMatrix() : glm::mat4(1.0f);
    }
    void setLocalMatrix(const glm::mat4& matrix) {
        if (m_transform) m_transform->setLocalMatrix(matrix);
    }

    // Hierarchy
    Asset3dInstance* parent = nullptr;  // Raw pointer (does not own)
    std::vector<std::unique_ptr<Asset3dInstance>> children;

    // Add child node with automatic parent assignment
    void addChild(Asset3dInstance* instance3d) {
        std::unique_ptr<Asset3dInstance> child(instance3d);
        child->parent = this;
        children.push_back(std::move(child));

        //TODO: ADICIONAR MÉTODO VIRTUAL AQUI. NO FILHO (FILAMENT) ELE DEVE PARENTEAR OS ENTITTIES DA FILAMENT.
    };

    // Type identification
    virtual bool isMesh() const { return false; }

    // Visibility control
    virtual void setVisible(bool visible) { m_visible = visible; }
    virtual bool isVisible() const { return m_visible; }
    
protected:
    std::unique_ptr<Transform> m_transform;
    bool m_visible = true;
    int m_id = -1;
};

} // namespace lite
