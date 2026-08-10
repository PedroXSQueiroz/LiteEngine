#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

#include <glm/glm.hpp>

namespace lite {

// Base do DTO de nó de cena, espelhando Asset3dInstance.
//
// Ao contrário do original, NÃO é template<TransformConcept>: o DTO não carrega
// transform concreto de backend nenhum, só a matriz.
//
// É polimórfica (mesh, câmera), então os filhos entram por ponteiro — vetor de
// valores faria slicing. O DTO é dono de si e morre inteiro, então o unique_ptr
// aqui não tem relação com a posse dos nós da Scene.
struct Asset3dInstanceDTO {
    virtual ~Asset3dInstanceDTO() = default;

    // Id no espaço de numeração da Scene. -1 = sem id.
    int32_t id = -1;

    std::string name;

    // Transformação LOCAL, relativa ao pai. A hierarquia está em children, então
    // gravar a global em cada nó seria redundante e ficaria inconsistente se um
    // pai mudasse.
    glm::mat4 localTransform = glm::mat4(1.0f);

    bool visible = true;

    std::vector<std::unique_ptr<Asset3dInstanceDTO>> children;
};

} // namespace lite
