#include <filament/data/assets/FilamentAsset3dTransform.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <optional>

namespace lite {

FilamentAsset3dTransform::FilamentAsset3dTransform(
    filament::TransformManager& transformManager,
    std::optional<utils::Entity> entity
)
    : m_transformManager(transformManager)
    , m_entity(entity)
{
}

void FilamentAsset3dTransform::modifyComponent(
    const glm::vec3* newPosition,
    const glm::quat* newRotation,
    const glm::vec3* newScale
) {
    glm::mat4 current = getLocalMatrix();

    glm::vec3 position, scale, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    glm::decompose(current, scale, rotation, position, skew, perspective);

    if (newPosition) position = *newPosition;
    if (newRotation) rotation = *newRotation;
    if (newScale) scale = *newScale;

    glm::mat4 result = glm::translate(glm::mat4(1.0f), position)
                     * glm::mat4_cast(rotation)
                     * glm::scale(glm::mat4(1.0f), scale);

    setLocalMatrix(result);
}

void FilamentAsset3dTransform::setPosition(const glm::vec3& position) {
    modifyComponent(&position, nullptr, nullptr);
}

glm::vec3 FilamentAsset3dTransform::getPosition() {
    glm::mat4 m = getLocalMatrix();
    return glm::vec3(m[3]);
}

void FilamentAsset3dTransform::setRotation(const glm::quat& rotation) {
    modifyComponent(nullptr, &rotation, nullptr);
}

glm::quat FilamentAsset3dTransform::getRotation() {
    glm::mat4 m = getLocalMatrix();
    glm::vec3 position, scale, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    glm::decompose(m, scale, rotation, position, skew, perspective);
    return rotation;
}

void FilamentAsset3dTransform::setScale(const glm::vec3& scale) {
    modifyComponent(nullptr, nullptr, &scale);
}

glm::vec3 FilamentAsset3dTransform::getScale() {
    glm::mat4 m = getLocalMatrix();
    glm::vec3 position, scale, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    glm::decompose(m, scale, rotation, position, skew, perspective);
    return scale;
}

void FilamentAsset3dTransform::setLocalMatrix(const glm::mat4& matrix) {
    this->assertEntity();
    
    auto instance = m_transformManager.getInstance(m_entity.value());
    if (!instance) return;

    filament::math::mat4f mat;
    std::memcpy(&mat, glm::value_ptr(matrix), sizeof(float) * 16);
    m_transformManager.setTransform(instance, mat);
}

glm::mat4 FilamentAsset3dTransform::getLocalMatrix() {
    this->assertEntity();
    
    auto instance = m_transformManager.getInstance(m_entity.value());
    if (!instance) return glm::mat4(1.0f);

    const auto& filamentMat = m_transformManager.getTransform(instance);
    glm::mat4 result;
    std::memcpy(glm::value_ptr(result), &filamentMat, sizeof(float) * 16);
    return result;
}

glm::mat4 FilamentAsset3dTransform::getWorldMatrix() {
    this->assertEntity();
    
    auto instance = m_transformManager.getInstance(m_entity.value());
    if (!instance) return glm::mat4(1.0f);

    const auto& filamentMat = m_transformManager.getWorldTransform(instance);
    glm::mat4 result;
    std::memcpy(glm::value_ptr(result), &filamentMat, sizeof(float) * 16);
    return result;
}

} // namespace lite
