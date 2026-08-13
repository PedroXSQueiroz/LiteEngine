#pragma once

#include <core/data/assets/IO/SceneSerializer.h>

#include <optional>
#include <string>

namespace lite {

// Implementação do facade de persistência sobre FlatBuffers.
//
// NENHUM tipo gerado pelo flatc aparece aqui: o schema (scene.fbs), o
// scene_generated.h e o runtime da biblioteca ficam todos dentro do .cpp. Este
// header fala apenas em SceneDTO e path, como o SceneSerializer exige — trocar
// a implementação não deve tocar em nada acima dela.
//
// O formato tem file_identifier "LSCN" e um schema_version dentro do arquivo:
// conteúdo que não seja nosso, ou de uma geração que esta build não saiba ler,
// falha o load em vez de ser interpretado errado.
class FlatBuffersSceneSerializer : public SceneSerializer {
public:
    ~FlatBuffersSceneSerializer() override = default;

    // false em qualquer falha de escrita (path inválido, disco cheio).
    bool save(const SceneDTO& scene, const std::string& path) override;

    // std::nullopt em qualquer falha de leitura: arquivo inexistente, conteúdo
    // que não passa na verificação do FlatBuffers ou identificador diferente, e
    // schema_version que esta build não sabe ler.
    std::optional<SceneDTO> load(const std::string& path) override;
};

} // namespace lite
