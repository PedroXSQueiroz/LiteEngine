#include <filament/data/assets/FilamentCameraAsset3dInstance.h>
#include <filament/TransformManager.h>
#include <utils/EntityManager.h>

namespace lite {

FilamentCameraAsset3dInstance::FilamentCameraAsset3dInstance(filament::Engine* engine)
    : m_engine(engine)
{
    // Create camera entity
    m_entity = utils::EntityManager::get().create();
    m_camera = engine->createCamera(m_entity);

    initializeTransform();
}

FilamentCameraAsset3dInstance::~FilamentCameraAsset3dInstance() {
    if (m_engine && m_camera) {
        m_engine->destroyCameraComponent(m_entity);
        utils::EntityManager::get().destroy(m_entity);
    }
}

void FilamentCameraAsset3dInstance::initializeTransform() {
    auto& tm = m_engine->getTransformManager();
    // Create transform component if it doesn't exist
    if (!tm.hasComponent(m_entity)) {
        tm.create(m_entity);
    }
    m_transform = std::make_unique<FilamentAsset3dTransform>(tm, m_entity);
}

glm::mat4 FilamentCameraAsset3dInstance::getViewMatrix() const {
    if (!m_camera) return glm::mat4(1.0f);

    auto view = m_camera->getViewMatrix(); // filament::math::mat4 (double)
    glm::mat4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result[i][j] = static_cast<float>(view[i][j]);
        }
    }
    return result;
}

glm::mat4 FilamentCameraAsset3dInstance::getProjectionMatrix() const {
    if (!m_camera) return glm::mat4(1.0f);

    auto proj = m_camera->getProjectionMatrix(); // filament::math::mat4 (double)
    glm::mat4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result[i][j] = static_cast<float>(proj[i][j]);
        }
    }
    return result;
}

void FilamentCameraAsset3dInstance::lookAt(const glm::vec3& center) {
    if (!m_camera || !m_transform) return;

    // Get eye position from transform (delegate to FilamentAsset3dTransform)
    glm::vec3 eye = m_transform->getPosition();

    // World up vector
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    // Convert GLM to Filament math types
    filament::math::float3 fEye(eye.x, eye.y, eye.z);
    filament::math::float3 fCenter(center.x, center.y, center.z);
    filament::math::float3 fUp(up.x, up.y, up.z);

    m_camera->lookAt(fEye, fCenter, fUp);
}

void FilamentCameraAsset3dInstance::setProjection(
    float fovDegrees,
    float aspect,
    float near,
    float far
) {
    if (!m_camera) return;
    m_camera->setProjection(fovDegrees, aspect, near, far);
}

} // namespace lite
