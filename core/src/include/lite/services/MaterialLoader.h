#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <memory>

#include <filament/Engine.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Texture.h>
#include <filament/TextureSampler.h>
#include <imageio/ImageDecoder.h>
#include <image/LinearImage.h>

#include <assimp/material.h>
#include <assimp/scene.h>

// Estrutura para guardar informações de um material
struct MaterialData {
    filament::Material* material = nullptr;
    filament::MaterialInstance* instance = nullptr;
    
    // Texturas
    filament::Texture* baseColorTexture = nullptr;
    filament::Texture* normalTexture = nullptr;
    filament::Texture* metallicRoughnessTexture = nullptr;
    filament::Texture* ambientOcclusionTexture = nullptr;
    filament::Texture* emissiveTexture = nullptr;
    
    // Fatores
    filament::math::float4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    filament::math::float3 emissiveFactor = {0.0f, 0.0f, 0.0f};
    
    std::string name;
};

class MaterialLoader {
public:
    MaterialLoader(filament::Engine* engine, const std::string& defaultMaterialPath = "")
        : m_engine(engine)
        , m_defaultMaterialPath(defaultMaterialPath)
    {
        if (m_defaultMaterialPath.empty()) {
            // Usar lit.filamat como padrão
            // m_defaultMaterialPath = "D:/Workspace/LiteEngine/3rd_party/filament/out/filament/generated/material/lit.filamat";
            m_defaultMaterialPath = "D:/Workspace/LiteEngine/core/resources/filament/materials/lit.filamat";
        }
    }
    
    ~MaterialLoader() {
        cleanup();
    }
    
    // Carregar material do Assimp
    MaterialData* loadFromAssimp(
        const aiMaterial* aiMat,
        const std::string& baseDirectory
    ) {
        if (!aiMat) return nullptr;
        
        // Obter nome do material
        aiString matName;
        aiMat->Get(AI_MATKEY_NAME, matName);
        std::string materialName = matName.C_Str();
        
        std::cout << "Loading material: " << materialName << std::endl;
        
        // Verificar se já foi carregado (cache)
        auto it = m_materialCache.find(materialName);
        if (it != m_materialCache.end()) {
            std::cout << "  Using cached material" << std::endl;
            return &it->second;
        }
        
        // Criar novo material
        MaterialData matData;
        matData.name = materialName;
        
        // Carregar base material
        matData.material = loadFilamentMaterial(m_defaultMaterialPath);
        if (!matData.material) {
            std::cerr << "  Failed to load base material!" << std::endl;
            return nullptr;
        }
        
        matData.instance = matData.material->createInstance();
        
        // Carregar texturas
        loadTextures(aiMat, baseDirectory, matData);
        
        // Carregar fatores/valores
        loadMaterialFactors(aiMat, matData);
        
        // Aplicar ao material instance
        applyToMaterialInstance(matData);
        
        // Adicionar ao cache
        m_materialCache[materialName] = matData;
        
        std::cout << "  Material loaded successfully!" << std::endl;
        
        return &m_materialCache[materialName];
    }
    
    // Criar material simples com cor
    MaterialData* createSimpleMaterial(
        const std::string& name,
        const filament::math::float4& color
    ) {
        auto it = m_materialCache.find(name);
        if (it != m_materialCache.end()) {
            return &it->second;
        }
        
        MaterialData matData;
        matData.name = name;
        matData.baseColorFactor = color;
        
        matData.material = loadFilamentMaterial(m_defaultMaterialPath);
        if (!matData.material) {
            return nullptr;
        }
        
        matData.instance = matData.material->createInstance();
        applyToMaterialInstance(matData);
        
        m_materialCache[name] = matData;
        return &m_materialCache[name];
    }
    
    // Obter material do cache
    MaterialData* getMaterial(const std::string& name) {
        auto it = m_materialCache.find(name);
        return (it != m_materialCache.end()) ? &it->second : nullptr;
    }
    
    // Limpar todos os recursos
    void cleanup() {
        std::cout << "Cleaning up MaterialLoader..." << std::endl;
        
        // Limpar texturas
        for (auto& [path, texture] : m_textureCache) {
            if (texture) {
                m_engine->destroy(texture);
            }
        }
        m_textureCache.clear();
        
        // Limpar materiais
        for (auto& [name, matData] : m_materialCache) {
            if (matData.instance) {
                m_engine->destroy(matData.instance);
            }
            if (matData.material) {
                m_engine->destroy(matData.material);
            }
        }
        m_materialCache.clear();
        
        std::cout << "  MaterialLoader cleaned up" << std::endl;
    }
    
    // Estatísticas
    void printStats() const {
        std::cout << "MaterialLoader Statistics:" << std::endl;
        std::cout << "  Materials: " << m_materialCache.size() << std::endl;
        std::cout << "  Textures: " << m_textureCache.size() << std::endl;
    }

private:
    filament::Engine* m_engine;
    std::string m_defaultMaterialPath;
    
    // Caches
    std::unordered_map<std::string, MaterialData> m_materialCache;
    std::unordered_map<std::string, filament::Texture*> m_textureCache;
    
    // Carregar arquivo .filamat
    filament::Material* loadFilamentMaterial(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open material: " << path << std::endl;
            return nullptr;
        }
        
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
        file.close();
        
        return filament::Material::Builder()
            .package(data.data(), data.size())
            .build(*m_engine);
    }
    
    // Carregar textura (ajustado para Filament 1.65.1)
    filament::Texture* loadTexture(const std::string& path, bool sRGB) {
        if (path.empty()) return nullptr;
        
        // Verificar cache
        auto it = m_textureCache.find(path);
        if (it != m_textureCache.end()) {
            std::cout << "    (cached) " << path << std::endl;
            return it->second;
        }
        
        std::cout << "    Loading: " << path << std::endl;
        
        // Carregar arquivo
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "    Failed to open: " << path << std::endl;
            return nullptr;
        }
        
        // Decodificar diretamente do stream (Filament 1.65.1)
        image::ImageDecoder::ColorSpace colorSpace = sRGB ? 
            image::ImageDecoder::ColorSpace::SRGB : 
            image::ImageDecoder::ColorSpace::LINEAR;
        
        image::LinearImage image = image::ImageDecoder::decode(
            file, path, colorSpace
        );
        
        file.close();
        
        uint32_t width = image.getWidth();
        uint32_t height = image.getHeight();
        uint32_t channels = image.getChannels();
        
        // Verificar se decodificou corretamente
        if (width == 0 || height == 0) {
            std::cerr << "    Failed to decode: " << path << std::endl;
            return nullptr;
        }
        
        // Determinar formato
        filament::Texture::InternalFormat format;
        if (sRGB) {
            format = (channels == 4) ? 
                filament::Texture::InternalFormat::SRGB8_A8 : 
                filament::Texture::InternalFormat::SRGB8;
        } else {
            format = (channels == 4) ? 
                filament::Texture::InternalFormat::RGBA8 : 
                filament::Texture::InternalFormat::RGB8;
        }
        
        // Criar textura
        filament::Texture* texture = filament::Texture::Builder()
            .width(width)
            .height(height)
            .levels(1)
            .sampler(filament::Texture::Sampler::SAMPLER_2D)
            .format(format)
            .build(*m_engine);
        
        // Preparar dados para upload
        size_t dataSize = width * height * channels;
        
        // Alocar memória e copiar os dados da imagem
        void* pixelDataCopy = malloc(dataSize);
        if (!pixelDataCopy) {
            std::cerr << "    Failed to allocate memory for texture data" << std::endl;
            m_engine->destroy(texture);
            return nullptr;
        }
        memcpy(pixelDataCopy, image.getPixelRef(), dataSize);
        
        // Criar buffer descriptor com callback de limpeza
        filament::Texture::PixelBufferDescriptor buffer(
            pixelDataCopy,
            dataSize,
            (channels == 4) ? filament::Texture::Format::RGBA : filament::Texture::Format::RGB,
            filament::Texture::Type::UBYTE,
            [](void* buffer, size_t size, void* user) {
                free(buffer);
            },
            nullptr
        );
        
        texture->setImage(*m_engine, 0, std::move(buffer));
        
        // Adicionar ao cache
        m_textureCache[path] = texture;
        
        std::cout << "    Loaded: " << width << "x" << height << " (" << channels << " ch)" << std::endl;
        
        return texture;
    }
    
    // Obter path de textura do material Assimp
    std::string getTexturePath(
        const aiMaterial* aiMat,
        aiTextureType type,
        const std::string& baseDirectory
    ) {
        if (aiMat->GetTextureCount(type) == 0) {
            return "";
        }
        
        aiString texPath;
        if (aiMat->GetTexture(type, 0, &texPath) != AI_SUCCESS) {
            return "";
        }
        
        std::string path = texPath.C_Str();
        
        // Se relativo, combinar com diretório base
        if (path[0] != '/' && !(path.length() > 1 && path[1] == ':')) {
            path = baseDirectory + "/" + path;
        }
        
        // Normalizar separadores
        std::replace(path.begin(), path.end(), '\\', '/');
        
        return path;
    }
    
    // Carregar todas as texturas
    void loadTextures(
        const aiMaterial* aiMat,
        const std::string& baseDirectory,
        MaterialData& matData
    ) {
        std::cout << "  Loading textures..." << std::endl;
        
        // Base Color / Albedo
        std::string path = getTexturePath(aiMat, aiTextureType_DIFFUSE, baseDirectory);
        if (path.empty()) {
            path = getTexturePath(aiMat, aiTextureType_BASE_COLOR, baseDirectory);
        }
        if (!path.empty()) {
            std::cout << "  BaseColor:" << std::endl;
            matData.baseColorTexture = loadTexture(path, true);
        }
        
        // Normal
        path = getTexturePath(aiMat, aiTextureType_NORMALS, baseDirectory);
        if (path.empty()) {
            path = getTexturePath(aiMat, aiTextureType_HEIGHT, baseDirectory);
        }
        if (!path.empty()) {
            std::cout << "  Normal:" << std::endl;
            matData.normalTexture = loadTexture(path, false);
        }
        
        // Metallic-Roughness (packed)
        std::string metallicPath = getTexturePath(aiMat, aiTextureType_METALNESS, baseDirectory);
        std::string roughnessPath = getTexturePath(aiMat, aiTextureType_DIFFUSE_ROUGHNESS, baseDirectory);
        
        if (!metallicPath.empty() && metallicPath == roughnessPath) {
            std::cout << "  MetallicRoughness (packed):" << std::endl;
            matData.metallicRoughnessTexture = loadTexture(metallicPath, false);
        } else if (!metallicPath.empty()) {
            std::cout << "  Metallic:" << std::endl;
            matData.metallicRoughnessTexture = loadTexture(metallicPath, false);
        }
        
        // Ambient Occlusion
        path = getTexturePath(aiMat, aiTextureType_AMBIENT_OCCLUSION, baseDirectory);
        if (path.empty()) {
            path = getTexturePath(aiMat, aiTextureType_LIGHTMAP, baseDirectory);
        }
        if (!path.empty()) {
            std::cout << "  AO:" << std::endl;
            matData.ambientOcclusionTexture = loadTexture(path, false);
        }
        
        // Emissive
        path = getTexturePath(aiMat, aiTextureType_EMISSIVE, baseDirectory);
        if (!path.empty()) {
            std::cout << "  Emissive:" << std::endl;
            matData.emissiveTexture = loadTexture(path, true);
        }
    }
    
    // Carregar valores/fatores do material
    void loadMaterialFactors(const aiMaterial* aiMat, MaterialData& matData) {
        std::cout << "  Loading material factors..." << std::endl;
        
        // Base Color
        aiColor3D color;
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            matData.baseColorFactor = {color.r, color.g, color.b, 1.0f};
            std::cout << "    BaseColor factor: (" << color.r << ", " << color.g << ", " << color.b << ")" << std::endl;
        }
        
        // Metallic - usar as chaves do Assimp 5.x+
        float metallic;
        // Tentar a chave direta do PBR
        aiReturn result = aiMat->Get("$mat.gltf.pbrMetallicRoughness.metallicFactor", 0, 0, metallic);
        if (result != AI_SUCCESS) {
            // Tentar alternativa
            result = aiMat->Get(AI_MATKEY_REFLECTIVITY, metallic);
        }
        if (result == AI_SUCCESS) {
            matData.metallicFactor = metallic;
            std::cout << "    Metallic: " << metallic << std::endl;
        }
        
        // Roughness
        float roughness;
        // Tentar a chave direta do PBR
        result = aiMat->Get("$mat.gltf.pbrMetallicRoughness.roughnessFactor", 0, 0, roughness);
        if (result != AI_SUCCESS) {
            // Converter shininess para roughness como fallback
            float shininess;
            if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
                // Shininess típico vai de 0 a 1000+
                // Roughness vai de 0 (liso) a 1 (áspero)
                roughness = 1.0f - std::min(shininess / 100.0f, 1.0f);
                std::cout << "    Roughness (converted from shininess " << shininess << "): " << roughness << std::endl;
            } else {
                roughness = 0.5f; // valor padrão
            }
        } else {
            std::cout << "    Roughness: " << roughness << std::endl;
        }
        matData.roughnessFactor = roughness;
        
        // Emissive
        aiColor3D emissive;
        if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
            matData.emissiveFactor = {emissive.r, emissive.g, emissive.b};
            std::cout << "    Emissive factor: (" << emissive.r << ", " << emissive.g << ", " << emissive.b << ")" << std::endl;
        }
    }
    
    // Aplicar ao MaterialInstance
    void applyToMaterialInstance(MaterialData& matData) {
        if (!matData.instance) return;
        
        std::cout << "  Applying to MaterialInstance..." << std::endl;
        
        // Sampler padrão
        filament::TextureSampler sampler(
            filament::TextureSampler::MinFilter::LINEAR_MIPMAP_LINEAR,
            filament::TextureSampler::MagFilter::LINEAR,
            filament::TextureSampler::WrapMode::REPEAT
        );
        
        // Aplicar texturas
        if (matData.baseColorTexture) {
            matData.instance->setParameter("baseColorMap", matData.baseColorTexture, sampler);
        }
        matData.instance->setParameter("baseColorFactor", matData.baseColorFactor);
        
        if (matData.normalTexture) {
            matData.instance->setParameter("normalMap", matData.normalTexture, sampler);
        }
        
        if (matData.metallicRoughnessTexture) {
            matData.instance->setParameter("metallicRoughnessMap", matData.metallicRoughnessTexture, sampler);
        }
        matData.instance->setParameter("metallicFactor", matData.metallicFactor);
        matData.instance->setParameter("roughnessFactor", matData.roughnessFactor);
        
        if (matData.ambientOcclusionTexture) {
            matData.instance->setParameter("occlusionMap", matData.ambientOcclusionTexture, sampler);
        }
        
        if (matData.emissiveTexture) {
            matData.instance->setParameter("emissiveMap", matData.emissiveTexture, sampler);
        }
        matData.instance->setParameter("emissiveFactor", matData.emissiveFactor);
        
        std::cout << "  Material ready!" << std::endl;
    }
};