#include <assimp/assets/importer/AssimpImporter.h>

#include <iostream>
#include <algorithm>
#include <limits>

namespace lite {

// Helper to count meshes recursively
static size_t countMeshes(const Asset3dData& node) {
    size_t count = node.isMesh() ? 1 : 0;
    for (const auto& child : node.children) {
        count += countMeshes(*child);
    }
    return count;
}

bool AssimpImporter::import(
    const std::string& filePath,
    Asset3dData& rootNode,
    std::vector<std::unique_ptr<MaterialData>>& materials
) {
    const aiScene* scene = m_importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs
    );

    if (!scene || !scene->HasMeshes()) {
        std::cerr << "AssimpImporter: Failed to load file: " << m_importer.GetErrorString() << std::endl;
        return false;
    }

    m_currentScene = scene;

    // Extract base directory for texture paths
    size_t lastSlash = filePath.find_last_of("/\\");
    std::string baseDirectory = (lastSlash != std::string::npos)
        ? filePath.substr(0, lastSlash)
        : ".";

    // Process all materials first
    materials.clear();
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        materials.push_back(processMaterial(scene->mMaterials[i], baseDirectory));
    }

    // Clear any existing children in rootNode
    rootNode.children.clear();
    rootNode.name = "root";
    rootNode.localTransform = glm::mat4(1.0f);

    // Process scene hierarchy starting from root
    processNode(scene->mRootNode, scene, rootNode, baseDirectory);

    std::cout << "AssimpImporter: Loaded " << filePath << std::endl;
    std::cout << "  Meshes: " << countMeshes(rootNode) << std::endl;
    std::cout << "  Materials: " << materials.size() << std::endl;

    m_currentScene = nullptr;
    return true;
}

bool AssimpImporter::canImport(const std::string& extension) const {
    auto extensions = getSupportedExtensions();
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Remove leading dot if present
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }

    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

std::vector<std::string> AssimpImporter::getSupportedExtensions() const {
    return {
        "fbx", "obj", "gltf", "glb", "dae", "3ds",
        "blend", "ase", "ifc", "xgl", "zgl", "ply",
        "lwo", "lws", "lxo", "stl", "x", "ac", "ms3d"
    };
}

void AssimpImporter::processNode(
    const aiNode* node,
    const aiScene* scene,
    Asset3dData& parentNode,
    const std::string& baseDirectory
) {
    // Determine the current node to add children to
    Asset3dData* currentNode = &parentNode;

    // If this is not the root node, create a container node
    if (node != scene->mRootNode) {
        currentNode = parentNode.addChild<Asset3dData>();
        currentNode->name = node->mName.C_Str();
        currentNode->localTransform = toGlmMatrix(node->mTransformation);
    }

    // Create MeshAsset3dData for each mesh in this node
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        auto* meshNode = currentNode->addChild<MeshAsset3dData>();
        meshNode->name = mesh->mName.C_Str();
        // Mesh inherits transform from parent, localTransform = identity

        // Populate geometry data
        populateMeshData(meshNode, mesh);

        // Material reference by name
        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            aiString matName;
            scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, matName);
            meshNode->materialName = matName.C_Str();
        }
    }

    // Process children recursively
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, *currentNode, baseDirectory);
    }
}

void AssimpImporter::populateMeshData(MeshAsset3dData* meshNode, const aiMesh* mesh) {
    // Initialize bounds
    glm::vec3 boundsMin(std::numeric_limits<float>::max());
    glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

    // Reserve space
    meshNode->positions.reserve(mesh->mNumVertices);
    meshNode->normals.reserve(mesh->mNumVertices);
    meshNode->uvs.reserve(mesh->mNumVertices);

    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        // Position
        glm::vec3 pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        meshNode->positions.push_back(pos);

        // Update bounds
        boundsMin = glm::min(boundsMin, pos);
        boundsMax = glm::max(boundsMax, pos);

        // Normal
        if (mesh->HasNormals()) {
            meshNode->normals.emplace_back(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        } else {
            meshNode->normals.emplace_back(0.0f, 1.0f, 0.0f);
        }

        // UV
        if (mesh->HasTextureCoords(0)) {
            meshNode->uvs.emplace_back(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        } else {
            meshNode->uvs.emplace_back(0.0f, 0.0f);
        }
    }

    // Process indices
    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices == 3) {
            meshNode->indices.push_back(face.mIndices[0]);
            meshNode->indices.push_back(face.mIndices[1]);
            meshNode->indices.push_back(face.mIndices[2]);
        }
    }

    // Calculate bounding data
    meshNode->boundsMin = boundsMin;
    meshNode->boundsMax = boundsMax;
    meshNode->center = (boundsMin + boundsMax) * 0.5f;
    meshNode->radius = glm::length(boundsMax - boundsMin) * 0.5f;
}

std::unique_ptr<MaterialData> AssimpImporter::processMaterial(
    const aiMaterial* material,
    const std::string& baseDirectory
) {
    // O Assimp só entrega parâmetros PBR metallic-roughness: todo material
    // importado é um MPBRLitMaterialData.
    MPBRLitMaterialData matData;

    // Get material name
    aiString matName;
    material->Get(AI_MATKEY_NAME, matName);
    matData.name = matName.C_Str();

    std::cout << "  Processing material: " << matData.name << std::endl;

    // Base color factor
    aiColor3D color;
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        matData.baseColorFactor = glm::vec4(color.r, color.g, color.b, 1.0f);
    }

    // Metallic factor
    float metallic = 0.0f;
    if (material->Get("$mat.gltf.pbrMetallicRoughness.metallicFactor", 0, 0, metallic) == AI_SUCCESS) {
        matData.metallicFactor = metallic;
    } else if (material->Get(AI_MATKEY_REFLECTIVITY, metallic) == AI_SUCCESS) {
        matData.metallicFactor = metallic;
    }

    // Roughness factor
    float roughness = 1.0f;
    if (material->Get("$mat.gltf.pbrMetallicRoughness.roughnessFactor", 0, 0, roughness) == AI_SUCCESS) {
        matData.roughnessFactor = roughness;
    } else {
        float shininess;
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
            matData.roughnessFactor = 1.0f - std::min(shininess / 100.0f, 1.0f);
        }
    }

    // Emissive factor
    aiColor3D emissive;
    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
        matData.emissiveFactor = glm::vec3(emissive.r, emissive.g, emissive.b);
    }

    // Base color texture
    std::string texPath = getTexturePath(material, aiTextureType_DIFFUSE, baseDirectory);
    if (texPath.empty()) {
        texPath = getTexturePath(material, aiTextureType_BASE_COLOR, baseDirectory);
    }
    if (!texPath.empty()) {
        TextureInfo texInfo;
        texInfo.path = texPath;
        texInfo.sRGB = true;
        matData.baseColorTexture = texInfo;
    }

    // Normal texture
    texPath = getTexturePath(material, aiTextureType_NORMALS, baseDirectory);
    if (texPath.empty()) {
        texPath = getTexturePath(material, aiTextureType_HEIGHT, baseDirectory);
    }
    if (!texPath.empty()) {
        TextureInfo texInfo;
        texInfo.path = texPath;
        texInfo.sRGB = false;
        matData.normalTexture = texInfo;
    }

    // Metallic-Roughness texture
    texPath = getTexturePath(material, aiTextureType_METALNESS, baseDirectory);
    if (!texPath.empty()) {
        TextureInfo texInfo;
        texInfo.path = texPath;
        texInfo.sRGB = false;
        matData.metallicRoughnessTexture = texInfo;
    }

    // Ambient occlusion texture
    texPath = getTexturePath(material, aiTextureType_AMBIENT_OCCLUSION, baseDirectory);
    if (texPath.empty()) {
        texPath = getTexturePath(material, aiTextureType_LIGHTMAP, baseDirectory);
    }
    if (!texPath.empty()) {
        TextureInfo texInfo;
        texInfo.path = texPath;
        texInfo.sRGB = false;
        matData.occlusionTexture = texInfo;
    }

    // Emissive texture
    texPath = getTexturePath(material, aiTextureType_EMISSIVE, baseDirectory);
    if (!texPath.empty()) {
        TextureInfo texInfo;
        texInfo.path = texPath;
        texInfo.sRGB = true;
        matData.emissiveTexture = texInfo;
    }

    return std::make_unique<MPBRLitMaterialData>(std::move(matData));
}

std::string AssimpImporter::getTexturePath(
    const aiMaterial* material,
    aiTextureType type,
    const std::string& baseDirectory
) {
    if (material->GetTextureCount(type) == 0) {
        return "";
    }

    aiString texPath;
    if (material->GetTexture(type, 0, &texPath) != AI_SUCCESS) {
        return "";
    }

    std::string path = texPath.C_Str();

    // If relative, combine with base directory
    if (!path.empty() && path[0] != '/' && !(path.length() > 1 && path[1] == ':')) {
        path = baseDirectory + "/" + path;
    }

    // Normalize separators
    std::replace(path.begin(), path.end(), '\\', '/');

    return path;
}

glm::mat4 AssimpImporter::toGlmMatrix(const aiMatrix4x4& m) {
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

} // namespace lite
