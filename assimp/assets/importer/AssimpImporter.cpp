#include <assimp/assets/importer/AssimpImporter.h>

#include <iostream>
#include <algorithm>
#include <limits>

namespace lite {

std::unique_ptr<Asset3dData> AssimpImporter::import(const std::string& filePath) {
    const aiScene* scene = m_importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs
    );

    if (!scene || !scene->HasMeshes()) {
        std::cerr << "AssimpImporter: Failed to load file: " << m_importer.GetErrorString() << std::endl;
        return nullptr;
    }

    auto data = std::make_unique<Asset3dData>();
    data->sourcePath = filePath;

    // Extract base directory for texture paths
    size_t lastSlash = filePath.find_last_of("/\\");
    std::string baseDirectory = (lastSlash != std::string::npos)
        ? filePath.substr(0, lastSlash)
        : ".";

    // Process all materials first
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        data->materials.push_back(processMaterial(scene->mMaterials[i], baseDirectory));
    }

    // Process all meshes
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        data->meshes.push_back(processMesh(scene->mMeshes[i], scene));
    }

    // Process scene hierarchy starting from root
    processNode(scene->mRootNode, scene, *data, -1, baseDirectory);

    std::cout << "AssimpImporter: Loaded " << filePath << std::endl;
    std::cout << "  Nodes: " << data->nodes.size() << std::endl;
    std::cout << "  Meshes: " << data->meshes.size() << std::endl;
    std::cout << "  Materials: " << data->materials.size() << std::endl;

    return data;
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
    Asset3dData& data,
    int32_t parentIndex,
    const std::string& baseDirectory
) {
    SceneNode sceneNode;
    sceneNode.name = node->mName.C_Str();
    sceneNode.localTransform = toGlmMatrix(node->mTransformation);
    sceneNode.parentIndex = parentIndex;

    // Store mesh indices
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        sceneNode.meshIndices.push_back(node->mMeshes[i]);
    }

    // Add this node to the data
    uint32_t currentNodeIndex = static_cast<uint32_t>(data.nodes.size());
    data.nodes.push_back(sceneNode);

    // Update parent's child indices
    if (parentIndex >= 0) {
        data.nodes[parentIndex].childIndices.push_back(currentNodeIndex);
    }

    // Process children
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, data, currentNodeIndex, baseDirectory);
    }
}

MeshData AssimpImporter::processMesh(const aiMesh* mesh, const aiScene* scene) {
    MeshData meshData;
    meshData.name = mesh->mName.C_Str();
    meshData.materialIndex = mesh->mMaterialIndex;

    // Initialize bounds
    glm::vec3 boundsMin(std::numeric_limits<float>::max());
    glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

    // Reserve space
    meshData.positions.reserve(mesh->mNumVertices);
    meshData.normals.reserve(mesh->mNumVertices);
    meshData.uvs.reserve(mesh->mNumVertices);

    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        // Position
        glm::vec3 pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        meshData.positions.push_back(pos);

        // Update bounds
        boundsMin = glm::min(boundsMin, pos);
        boundsMax = glm::max(boundsMax, pos);

        // Normal
        if (mesh->HasNormals()) {
            meshData.normals.emplace_back(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        } else {
            meshData.normals.emplace_back(0.0f, 1.0f, 0.0f);
        }

        // UV
        if (mesh->HasTextureCoords(0)) {
            meshData.uvs.emplace_back(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        } else {
            meshData.uvs.emplace_back(0.0f, 0.0f);
        }
    }

    // Process indices
    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices == 3) {
            meshData.indices.push_back(face.mIndices[0]);
            meshData.indices.push_back(face.mIndices[1]);
            meshData.indices.push_back(face.mIndices[2]);
        }
    }

    // Calculate bounding data
    meshData.boundsMin = boundsMin;
    meshData.boundsMax = boundsMax;
    meshData.center = (boundsMin + boundsMax) * 0.5f;
    meshData.radius = glm::length(boundsMax - boundsMin) * 0.5f;

    return meshData;
}

MaterialData AssimpImporter::processMaterial(
    const aiMaterial* material,
    const std::string& baseDirectory
) {
    MaterialData matData;

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

    return matData;
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
