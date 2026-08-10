#pragma once

#include <core/data/DTOs/Asset3dInstanceDTO.h>

#include <glm/glm.hpp>

namespace lite {

// DTO de CameraAsset3dInstance.
//
// A câmera é um Asset3dInstance por tipo, mas hoje NÃO é nó da cena: pertence ao
// SceneRenderer e tem id -1. Mesmo assim ela entra dentro de
// SceneDTO::instances, por decisão — não em campo próprio da cena.
struct CameraAsset3dInstanceDTO : Asset3dInstanceDTO {
    glm::vec3 eye    = glm::vec3(0.0f);
    glm::vec3 target = glm::vec3(0.0f);
};

} // namespace lite
