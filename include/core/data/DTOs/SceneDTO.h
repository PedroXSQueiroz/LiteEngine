#pragma once

#include <core/data/DTOs/Asset3dInstanceDTO.h>
#include <core/data/DTOs/MaterialDTO.h>

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace lite {

// FIXME: o IBL ainda NÃO é agnóstico. O path carrega implicitamente o formato
// que a implementação concreta sabe ler (diretório de KTX / equirect no caso da
// Filament), e a posse hoje é do SceneRenderer, não da Scene. Rever quando o IBL
// for abstraído — a questão de posse tem de ser resolvida junto.
struct IblDTO {
    std::string path;
    float       intensity = 30000.0f;
};

// DTO da cena inteira — raiz do arquivo serializado.
struct SceneDTO {
    // Geração do formato. O FlatBuffers evolui campos sozinho, mas não diz de
    // qual geração o arquivo é nem se ele é nosso.
    uint32_t schemaVersion = 1;

    int32_t id = -1;

    IblDTO ibl;

    std::vector<std::unique_ptr<MaterialDTO>> materials;

    // Raízes da cena. A câmera entra aqui como CameraAsset3dInstanceDTO.
    // Luzes ainda NÃO entram: viram filhas de Asset3dInstance em breve, e aí
    // passam a caber neste mesmo vetor.
    std::vector<std::unique_ptr<Asset3dInstanceDTO>> instances;
};

} // namespace lite
