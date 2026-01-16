#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

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

// Material data (PBR properties)
struct MaterialData {
    std::string name;  // Unique identifier for lookup

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

} // namespace lite
