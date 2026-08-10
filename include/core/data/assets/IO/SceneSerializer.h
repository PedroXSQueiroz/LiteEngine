#pragma once

#include <core/data/DTOs/SceneDTO.h>

#include <optional>
#include <string>

namespace lite {

// Facade de persistência de cena.
//
// O core não conhece o formato do arquivo nem a biblioteca que o produz: nenhum
// tipo da implementação concreta (FlatBuffers hoje) aparece nesta interface, e
// trocar a implementação não deve tocar em nada que esteja acima dela.
//
// Fala APENAS em DTO. Converter Scene <-> SceneDTO é responsabilidade de outra
// peça — o serializador não conhece a Scene.
class SceneSerializer {
public:
    virtual ~SceneSerializer() = default;

    // Retorna false em qualquer falha de escrita (path inválido, disco cheio).
    virtual bool save(const SceneDTO& scene, const std::string& path) = 0;

    // std::nullopt em qualquer falha de leitura: arquivo inexistente, conteúdo
    // que não é do formato esperado, ou schemaVersion que esta build não sabe
    // ler.
    virtual std::optional<SceneDTO> load(const std::string& path) = 0;
};

} // namespace lite
