#pragma once

#include <core/data/DTOs/Asset3dInstanceDTO.h>

#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

namespace lite {

// DTO de MeshAsset3dInstance: a geometria vem embutida no próprio nó.
//
// ATALHO CONSCIENTE: enquanto não existir o gerenciador/cache de Asset3dData,
// cada instância carrega a própria cópia dos vértices. Quando o manager existir,
// isto vira uma referência ao id do Asset3dData e a geometria sai daqui.
struct MeshAsset3dInstanceDTO : Asset3dInstanceDTO {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;

    // ATENÇÃO: sai VAZIO por enquanto. A instância não guarda UVs CPU-side
    // (não existe cpuUvs em FilamentMeshAsset3dInstance), então todo save perde
    // as UVs até isso ser resolvido — ou adicionando cpuUvs, ou montando o DTO
    // a partir do Asset3dData em vez da instância.
    std::vector<glm::vec2> uvs;

    std::vector<uint32_t> indices;

    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);

    // Referência ao material por NOME, como MeshAsset3dData::materialName —
    // resolve contra SceneDTO::materials.
    std::string materialName;
};

} // namespace lite
