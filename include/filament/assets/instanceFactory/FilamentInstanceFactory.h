#pragma once

#include <core/assets/instanceFactory/Asset3dInstanceFactory.h>
#include <core/data/assets/Asset3dData.h>
#include <filament/assets/instanceFactory/FilamentAsset3dInstance.h>

#include <string>
#include <unordered_map>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/Material.h>
#include <filament/Texture.h>

namespace lite {

    class FilamentInstanceFactory : public Asset3dInstanceFactory {
    public:
        FilamentInstanceFactory(
            filament::Engine* engine,
            filament::Scene* scene,
            const std::string& defaultMaterialPath = ""
        );

        ~FilamentInstanceFactory() override;

        std::unique_ptr<Asset3dInstance> instantiate(const Asset3dData& data) override;
        void destroy(Asset3dInstance* instance) override;

        // Cleanup all cached resources
        void cleanup();

    private:
        // Create vertex buffer from mesh data
        filament::VertexBuffer* createVertexBuffer(const MeshData& mesh);

        // Create index buffer from mesh data
        filament::IndexBuffer* createIndexBuffer(const MeshData& mesh);

        // Load or get cached texture
        filament::Texture* loadTexture(const TextureInfo& texInfo);

        // Create material instance from material data
        filament::MaterialInstance* createMaterialInstance(const MaterialData& matData);

        // Process a node and create entities
        void processNode(
            const Asset3dData& data,
            uint32_t nodeIndex,
            FilamentAsset3dInstance& instance,
            const glm::mat4& parentTransform
        );

        // GLM to Filament type conversions
        static filament::math::float3 toFilament(const glm::vec3& v);
        static filament::math::float4 toFilament(const glm::vec4& v);
        static filament::math::mat4f toFilament(const glm::mat4& m);

        filament::Engine* m_engine;
        filament::Scene* m_scene;
        std::string m_defaultMaterialPath;

        // Base material (loaded once)
        filament::Material* m_baseMaterial = nullptr;

        // Caches
        std::unordered_map<std::string, filament::Texture*> m_textureCache;
    };

} // namespace lite
