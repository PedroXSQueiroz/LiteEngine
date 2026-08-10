#pragma once

#include <string>
#include <optional>
#include <cstdint>

#include <glm/glm.hpp>

namespace lite {

// DTO de TextureInfo. Só o path e os metadados de imagem — o embeddedData do
// original ficou DE FORA por decisão: textura embutida no arquivo de origem não
// sobrevive ao round-trip e depende de reimportar o arquivo original.
struct TextureInfoDTO {
    std::string path;
    bool     sRGB     = true;
    uint32_t width    = 0;
    uint32_t height   = 0;
    uint32_t channels = 0;
};

// Base do DTO de material, espelhando a divisão de MaterialData: só o que todo
// material tem, qualquer que seja o modelo de shading. Os parâmetros de cada
// modelo ficam nas filhas.
//
// É polimórfica pelo mesmo motivo do original — quem transporta uma coleção usa
// std::vector<std::unique_ptr<MaterialDTO>>, porque vetor de valores faria
// slicing e perderia os parâmetros da filha.
struct MaterialDTO {
    virtual ~MaterialDTO() = default;

    std::string name;  // identificador usado para casar com MeshAsset3dInstanceDTO::materialName
};

// DTO de MPBRLitMaterialData — modelo metallic-roughness com iluminação, o que
// o lit.filamat implementa.
struct MPBRLitMaterialDTO : MaterialDTO {
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float     metallicFactor  = 0.0f;
    float     roughnessFactor = 1.0f;
    glm::vec3 emissiveFactor  = glm::vec3(0.0f);

    std::optional<TextureInfoDTO> baseColorTexture;
    std::optional<TextureInfoDTO> normalTexture;
    std::optional<TextureInfoDTO> metallicRoughnessTexture;
    std::optional<TextureInfoDTO> occlusionTexture;
    std::optional<TextureInfoDTO> emissiveTexture;
};

} // namespace lite
