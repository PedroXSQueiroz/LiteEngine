#pragma once

#include <core/assets/importer/Asset3dImporter.h>
#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MeshAsset3dData.h>
#include <core/data/assets/MaterialData.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace lite {

class AssimpImporter : public Asset3dImporter {
public:
    AssimpImporter() = default;
    ~AssimpImporter() override = default;

    bool import(
        const std::string& filePath,
        Asset3dData& rootNode,
        std::vector<MaterialData>& materials
    ) override;

    bool canImport(const std::string& extension) const override;
    std::vector<std::string> getSupportedExtensions() const override;

private:
    // Process scene hierarchy recursively
    void processNode(
        const aiNode* node,
        const aiScene* scene,
        Asset3dData& parentNode,
        const std::string& baseDirectory
    );

    // Populate mesh data from Assimp mesh
    void populateMeshData(MeshAsset3dData* meshNode, const aiMesh* mesh);

    // Process a material
    MaterialData processMaterial(
        const aiMaterial* material,
        const std::string& baseDirectory
    );

    // Get texture path from Assimp material
    std::string getTexturePath(
        const aiMaterial* material,
        aiTextureType type,
        const std::string& baseDirectory
    );

    // Convert Assimp matrix to GLM
    glm::mat4 toGlmMatrix(const aiMatrix4x4& m);

    Assimp::Importer m_importer;

    // Store scene pointer during import for material name lookup
    const aiScene* m_currentScene = nullptr;
};

} // namespace lite
