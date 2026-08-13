#pragma once

#include <core/data/assets/IO/SceneSerializer.h>

#include <optional>
#include <string>

namespace lite {

// Implementação de VERIFICAÇÃO: não escreve nem lê arquivo nenhum.
//
// Existe só para conferir se o que os mappers produzem faz sentido: o save()
// despeja o SceneDTO inteiro no stdout (materiais, hierarquia de instâncias e os
// campos de cada tipo concreto) e devolve true. O load() não tem de onde ler e
// devolve sempre std::nullopt.
class DummySceneSerializer : public SceneSerializer {
public:
    ~DummySceneSerializer() override = default;

    // Imprime o DTO. O path é apenas ecoado no cabeçalho — nada é gravado.
    bool save(const SceneDTO& scene, const std::string& path) override;

    // Sempre std::nullopt: não existe arquivo para ler.
    std::optional<SceneDTO> load(const std::string& path) override;
};

} // namespace lite
