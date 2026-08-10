#pragma once

#include <core/data/assets/MPBRLitMaterialInstance.h>

#include <filament/MaterialInstance.h>

#include <glm/glm.hpp>

namespace lite {

// Wrapper NÃO-DONO de uma filament::MaterialInstance: cada getter/setter é uma
// chamada direta de getParameter/setParameter no objeto envolvido, sem espelho
// CPU dos valores. Quem cria e destrói a instância da Filament continua sendo a
// factory (FilamentInstanceFactory).
//
// THREAD: render thread, como todo o resto da Filament. Além disso, um
// setParameter só chega à GPU no commit que FEngine::prepare() faz no
// beginFrame seguinte.
//
// ATENÇÃO: uma mesma filament::MaterialInstance é compartilhada por várias
// meshes (a factory cria uma por MaterialData e distribui por nome), então um
// setter aqui edita todas elas de uma vez.
class FilamentMPBRLitMaterialInstance : public MPBRLitMaterialInstance {
public:
    explicit FilamentMPBRLitMaterialInstance(filament::MaterialInstance* instance);

    ~FilamentMPBRLitMaterialInstance() override = default;

    glm::vec4 getBaseColorFactor() const override;
    void      setBaseColorFactor(const glm::vec4& value) override;

    float getMetallicFactor() const override;
    void  setMetallicFactor(float value) override;

    float getRoughnessFactor() const override;
    void  setRoughnessFactor(float value) override;

    glm::vec3 getEmissiveFactor() const override;
    void      setEmissiveFactor(const glm::vec3& value) override;

    filament::MaterialInstance* getFilamentInstance() const { return m_instance; }

private:
    filament::MaterialInstance* m_instance;  // emprestado: NÃO destruir aqui
};

} // namespace lite
