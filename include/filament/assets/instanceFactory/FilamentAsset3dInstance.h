#pragma once

#include <core/data/assets/Asset3dInstance.h>

#include <vector>
#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Texture.h>
#include <utils/Entity.h>

#include <glm/glm.hpp>

namespace lite {

    // Filament-specific instance of a 3D asset
    class FilamentAsset3dInstance : public Asset3dInstance {
    public:
        FilamentAsset3dInstance(filament::Engine* engine, filament::Scene* scene)
            : m_engine(engine)
            , m_scene(scene)
            , m_visible(true)
            , m_transform(1.0f)
        {}

        ~FilamentAsset3dInstance() override = default;

        void setTransform(const glm::mat4& transform) override;
        void setVisible(bool visible) override;
        glm::mat4 getTransform() const override { return m_transform; }
        bool isVisible() const override { return m_visible; }

        // Filament resources
        std::vector<utils::Entity> entities;
        std::vector<filament::VertexBuffer*> vertexBuffers;
        std::vector<filament::IndexBuffer*> indexBuffers;
        std::vector<filament::MaterialInstance*> materialInstances;
        std::vector<filament::Texture*> textures;

        // Root entity for the whole asset (for hierarchy transforms)
        utils::Entity rootEntity;

    private:
        filament::Engine* m_engine;
        filament::Scene* m_scene;
        bool m_visible;
        glm::mat4 m_transform;
    };

} // namespace lite
