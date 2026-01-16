#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lite {

    // Forward declaration
    struct Asset3dImportData;

    // Texture information (renderer-agnostic)
    struct TextureInfo {
        std::string path;                   // Relative or absolute path
        std::vector<uint8_t> embeddedData;  // Embedded data (optional)
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 0;
        bool sRGB = true;                   // Is sRGB color space
    };

    // Material data (PBR properties)
    struct MaterialImportData {
        std::string name;

        // PBR factors
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        float metallicFactor = 0.0f;
        float roughnessFactor = 1.0f;
        glm::vec3 emissiveFactor = glm::vec3(0.0f);

        // Textures (optional)
        std::optional<TextureInfo> baseColorTexture;
        std::optional<TextureInfo> normalTexture;
        std::optional<TextureInfo> metallicRoughnessTexture;
        std::optional<TextureInfo> occlusionTexture;
        std::optional<TextureInfo> emissiveTexture;
    };

    // Mesh geometry data
    struct MeshImportData {
        std::string name;

        // Geometry
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<uint32_t> indices;

        // Bounding box
        glm::vec3 boundsMin = glm::vec3(0.0f);
        glm::vec3 boundsMax = glm::vec3(0.0f);
        glm::vec3 center = glm::vec3(0.0f);
        float radius = 0.0f;

        // Material reference
        int32_t materialIndex = -1;
    };

    // Scene node (hierarchy)
    struct SceneNode {
        std::string name;
        glm::mat4 localTransform = glm::mat4(1.0f);     // Transform relative to parent
        std::vector<uint32_t> meshIndices;               // Indices of meshes in this node
        std::vector<uint32_t> childIndices;              // Indices of child nodes
        int32_t parentIndex = -1;                        // -1 = root
    };

    // Main container for imported 3D asset data
    class Asset3dImportData {
    public:
        std::vector<SceneNode> nodes;
        std::vector<MeshImportData> meshes;
        std::vector<MaterialImportData> materials;

        uint32_t rootNodeIndex = 0;
        std::string sourcePath;

        // Helper: get root node
        const SceneNode& getRootNode() const {
            return nodes[rootNodeIndex];
        }

        // Helper: calculate world transform for a node
        glm::mat4 getWorldTransform(uint32_t nodeIndex) const {
            if (nodeIndex >= nodes.size()) {
                return glm::mat4(1.0f);
            }

            const SceneNode& node = nodes[nodeIndex];
            if (node.parentIndex < 0) {
                return node.localTransform;
            }

            return getWorldTransform(node.parentIndex) * node.localTransform;
        }

        // Helper: get total mesh count
        size_t getTotalMeshCount() const {
            return meshes.size();
        }

        // Helper: get total material count
        size_t getTotalMaterialCount() const {
            return materials.size();
        }
    };

} // namespace lite
