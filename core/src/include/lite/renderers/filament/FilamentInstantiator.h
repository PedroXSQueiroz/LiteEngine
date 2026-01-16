#pragma once

#include <lite/core/Asset3dInstantiator.h>
#include <lite/core/Asset3dImportData.h>
#include <lite/renderers/filament/FilamentAsset3dInstance.h>

#include <string>
#include <unordered_map>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/Material.h>
#include <filament/Texture.h>

namespace lite {

    class FilamentInstantiator : public Asset3dInstantiator {
    public:
        FilamentInstantiator(
            filament::Engine* engine,
            filament::Scene* scene,
            const std::string& defaultMaterialPath = ""
        );

        ~FilamentInstantiator() override;

        std::unique_ptr<Asset3dInstance> instantiate(const Asset3dImportData& data) override;
        void destroy(Asset3dInstance* instance) override;

        // Cleanup all cached resources
        void cleanup();

    private:
        // Create vertex buffer from mesh data
        filament::VertexBuffer* createVertexBuffer(const MeshImportData& mesh);

        // Create index buffer from mesh data
        filament::IndexBuffer* createIndexBuffer(const MeshImportData& mesh);

        // Load or get cached texture
        filament::Texture* loadTexture(const TextureInfo& texInfo);

        // Create material instance from material data
        filament::MaterialInstance* createMaterialInstance(const MaterialImportData& matData);

        // Process a node and create entities
        void processNode(
            const Asset3dImportData& data,
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
