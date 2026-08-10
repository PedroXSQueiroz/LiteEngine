#include <filament/data/assets/FilamentMPBRLitMaterialInstance.h>

namespace lite {

namespace {

glm::vec3 toGlm(const filament::math::float3& v) { return {v.x, v.y, v.z}; }
glm::vec4 toGlm(const filament::math::float4& v) { return {v.x, v.y, v.z, v.w}; }

filament::math::float3 toFilament(const glm::vec3& v) { return {v.x, v.y, v.z}; }
filament::math::float4 toFilament(const glm::vec4& v) { return {v.x, v.y, v.z, v.w}; }

} // namespace

FilamentMPBRLitMaterialInstance::FilamentMPBRLitMaterialInstance(
    filament::MaterialInstance* instance
)
    : m_instance(instance)
{
    // O nome vem do objeto envolvido: o wrapper não guarda estado próprio.
    m_name = instance ? instance->getName() : "";
}

glm::vec4 FilamentMPBRLitMaterialInstance::getBaseColorFactor() const {
    if (!m_instance) return glm::vec4(1.0f);
    return toGlm(m_instance->getParameter<filament::math::float4>("baseColorFactor"));
}

void FilamentMPBRLitMaterialInstance::setBaseColorFactor(const glm::vec4& value) {
    if (!m_instance) return;
    m_instance->setParameter("baseColorFactor", toFilament(value));
}

float FilamentMPBRLitMaterialInstance::getMetallicFactor() const {
    if (!m_instance) return 0.0f;
    return m_instance->getParameter<float>("metallicFactor");
}

void FilamentMPBRLitMaterialInstance::setMetallicFactor(float value) {
    if (!m_instance) return;
    m_instance->setParameter("metallicFactor", value);
}

float FilamentMPBRLitMaterialInstance::getRoughnessFactor() const {
    if (!m_instance) return 1.0f;
    return m_instance->getParameter<float>("roughnessFactor");
}

void FilamentMPBRLitMaterialInstance::setRoughnessFactor(float value) {
    if (!m_instance) return;
    m_instance->setParameter("roughnessFactor", value);
}

glm::vec3 FilamentMPBRLitMaterialInstance::getEmissiveFactor() const {
    if (!m_instance) return glm::vec3(0.0f);
    return toGlm(m_instance->getParameter<filament::math::float3>("emissiveFactor"));
}

void FilamentMPBRLitMaterialInstance::setEmissiveFactor(const glm::vec3& value) {
    if (!m_instance) return;
    m_instance->setParameter("emissiveFactor", toFilament(value));
}

} // namespace lite
