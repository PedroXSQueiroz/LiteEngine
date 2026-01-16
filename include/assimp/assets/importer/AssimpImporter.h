#pragma once

#include <core/assets/importer/Asset3dImporter.h>
#include <core/data/assets/Asset3dData.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace lite {

    class AssimpImporter : public Asset3dImporter {
    public:
        AssimpImporter() = default;
        ~AssimpImporter() override = default;

        std::unique_ptr<Asset3dData> import(const std::string& filePath) override;
        bool canImport(const std::string& extension) const override;
        std::vector<std::string> getSupportedExtensions() const override;

    private:
        // Process scene hierarchy recursively
        void processNode(
            const aiNode* node,
            const aiScene* scene,
            Asset3dData& data,
            int32_t parentIndex,
            const std::string& baseDirectory
        );

        // Process a single mesh
        MeshData processMesh(const aiMesh* mesh, const aiScene* scene);

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
    };

} // namespace lite
