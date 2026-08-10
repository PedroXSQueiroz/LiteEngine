#pragma once

#include <core/assets/instanceFactory/Asset3dInstanceFactory.h>
#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MeshAsset3dData.h>
#include <core/data/assets/MaterialData.h>
#include <filament/data/assets/FilamentAsset3dInstance.h>
#include <filament/data/assets/FilamentMeshAsset3dInstance.h>

#include <string>
#include <unordered_map>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/Material.h>
#include <filament/Texture.h>

namespace lite {

class FilamentInstanceFactory : public Asset3dInstanceFactory<FilamentAsset3dInstance, FilamentAsset3dTransform> {
public:
    FilamentInstanceFactory(
        filament::Engine* engine,
        filament::Scene* scene,
        const std::string& defaultMaterialPath = ""
    );

    ~FilamentInstanceFactory() override;

    std::unique_ptr<FilamentAsset3dInstance> instantiateAsset(
        const Asset3dData& rootNode,
        FilamentAsset3dTransform transform,
        const std::vector<std::unique_ptr<MaterialData>>& materials
    ) override;

    bool destroyAsset(FilamentAsset3dInstance *instance) override;

    
    // Cleanup all cached resources
    void cleanup();
    
    void flushDeletedFilament3dAssets();

private:
    
    std::vector<lite::FilamentAsset3dInstance *> m_deleted3dInstances;

    void destroyFilament3dAsset(lite::FilamentAsset3dInstance *filamentInstance);
    
    // Create vertex buffer from mesh data
    filament::VertexBuffer* createVertexBuffer(const MeshAsset3dData& mesh);

    // Create index buffer from mesh data
    filament::IndexBuffer* createIndexBuffer(const MeshAsset3dData& mesh);

    // Load or get cached texture
    filament::Texture* loadTexture(const TextureInfo& texInfo);

    // Create material instance from material data
    filament::MaterialInstance* createMaterialInstance(const MaterialData& matData);

    // Process a node recursively and create instance hierarchy
    void processNode(
        const Asset3dData& node,
        Asset3dInstance<FilamentAsset3dTransform>& parentInstance,
        const std::unordered_map<std::string, filament::MaterialInstance*>& materialMap
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
