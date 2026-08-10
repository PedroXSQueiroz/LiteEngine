#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

namespace lite {

// Texture information (renderer-agnostic)
struct TextureInfo {
    std::string path;                   // Relative or absolute path
    std::vector<uint8_t> embeddedData;  // Embedded data (optional)
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    bool sRGB = true;                   // Is sRGB color space
};

// Base de material agnóstica: só o que TODO material tem, qualquer que seja o
// modelo de shading. Os parâmetros de cada modelo ficam nas filhas (o único
// modelo existente hoje é MPBRLitMaterialData).
//
// É polimórfica, então o pipeline transporta os materiais por ponteiro
// (std::vector<std::unique_ptr<MaterialData>>) — vetor de valores faria
// slicing e perderia os parâmetros da filha.
struct MaterialData {
    virtual ~MaterialData() = default;

    std::string name;  // Unique identifier for lookup

    // Cópia profunda polimórfica, mesmo padrão de Asset3dData::clone(). A fila
    // de criação da Scene guarda uma cópia própria dos materiais recebidos em
    // create(), e unique_ptr não é copiável.
    virtual std::unique_ptr<MaterialData> clone() const {
        auto copy = std::make_unique<MaterialData>();
        copy->name = name;
        return copy;
    }
};

// Material data (PBR properties) — modelo metallic-roughness com iluminação,
// o que o lit.filamat implementa.
struct MPBRLitMaterialData : MaterialData {
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

    std::unique_ptr<MaterialData> clone() const override {
        auto copy = std::make_unique<MPBRLitMaterialData>();
        copy->name = name;
        copy->baseColorFactor = baseColorFactor;
        copy->metallicFactor = metallicFactor;
        copy->roughnessFactor = roughnessFactor;
        copy->emissiveFactor = emissiveFactor;
        copy->baseColorTexture = baseColorTexture;
        copy->normalTexture = normalTexture;
        copy->metallicRoughnessTexture = metallicRoughnessTexture;
        copy->occlusionTexture = occlusionTexture;
        copy->emissiveTexture = emissiveTexture;
        return copy;
    }
};

} // namespace lite
