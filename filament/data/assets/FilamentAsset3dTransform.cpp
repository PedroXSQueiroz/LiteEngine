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
    const glm::vec3* newScale,
    const bool isWorldSpace
) {
    glm::mat4 current = isWorldSpace ? getWorldMatrix(): getLocalMatrix();

    glm::vec3 position, scale, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    if( glm::decompose(current, scale, rotation, position, skew, perspective) ) 
    {
        if (newPosition) position = *newPosition;
        if (newRotation) rotation = *newRotation;
        if (newScale) scale = *newScale;
    
        glm::mat4 result = glm::translate(glm::mat4(1.0f), position)
                         * glm::mat4_cast(rotation)
                         * glm::scale(glm::mat4(1.0f), scale);
    
        if(isWorldSpace)
            setWorldMatrix(result);
        else    
            setLocalMatrix(result);
    }
}

void FilamentAsset3dTransform::setPosition(const glm::vec3& position, bool isWorldSpace) {
    modifyComponent(&position, nullptr, nullptr, isWorldSpace);
}

glm::vec3 FilamentAsset3dTransform::getPosition(bool isWorldSpace) {
    glm::mat4 m = isWorldSpace? getWorldMatrix(): getLocalMatrix();
    return glm::vec3(m[3]);
}

void FilamentAsset3dTransform::setRotation(const glm::quat& rotation, bool isWorldSpace) {
    modifyComponent(nullptr, &rotation, nullptr, isWorldSpace);
}

glm::quat FilamentAsset3dTransform::getRotation(bool isWorldSpace) {
    glm::mat4 m = isWorldSpace ? getWorldMatrix() : getLocalMatrix();
    glm::vec3 position, scale, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    glm::decompose(m, scale, rotation, position, skew, perspective);
    return rotation;
}

void FilamentAsset3dTransform::setScale(const glm::vec3& scale, bool isWorldSpace) {
    modifyComponent(nullptr, nullptr, &scale, isWorldSpace);
}

glm::vec3 FilamentAsset3dTransform::getScale(bool isWorldSpace) {
    glm::mat4 m = isWorldSpace ? getWorldMatrix() : getLocalMatrix();
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

void FilamentAsset3dTransform::setWorldMatrix(const glm::mat4& matrix) {
    this->assertEntity();

    auto instance = m_transformManager.getInstance(m_entity.value());
    if (!instance) return;

    // Filament só tem setter LOCAL → converte world → local:
    //   local = inverse(parentWorld) · world
    glm::mat4 localMatrix = matrix;

    utils::Entity parent = m_transformManager.getParent(instance);
    if (!parent.isNull()) {                 // tem pai → não é raiz
        auto parentInstance = m_transformManager.getInstance(parent);
        if (parentInstance) {
            const auto& pw = m_transformManager.getWorldTransform(parentInstance);
            glm::mat4 parentWorld;
            std::memcpy(glm::value_ptr(parentWorld), &pw, sizeof(float) * 16);
            localMatrix = glm::inverse(parentWorld) * matrix;
        }
    }

    setLocalMatrix(localMatrix);   
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
