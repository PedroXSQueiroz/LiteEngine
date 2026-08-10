#pragma once

#include <core/data/assets/MaterialInstance.h>

#include <glm/glm.hpp>

namespace lite {

// Contrato de edição em runtime dos parâmetros de MPBRLitMaterialData.
//
// THREAD: todos os métodos são contrato do CHAMADOR estar na render thread —
// a implementação mexe direto nos recursos do backend, sem enfileirar nada.
//
// As cinco texturas do Data ainda NÃO estão neste contrato (decidido: sem set
// de textura por enquanto).
class MPBRLitMaterialInstance : public MaterialInstance {
public:
    ~MPBRLitMaterialInstance() override = default;

    virtual glm::vec4 getBaseColorFactor() const = 0;
    virtual void      setBaseColorFactor(const glm::vec4& value) = 0;

    virtual float getMetallicFactor() const = 0;
    virtual void  setMetallicFactor(float value) = 0;

    virtual float getRoughnessFactor() const = 0;
    virtual void  setRoughnessFactor(float value) = 0;

    virtual glm::vec3 getEmissiveFactor() const = 0;
    virtual void      setEmissiveFactor(const glm::vec3& value) = 0;
};

} // namespace lite
