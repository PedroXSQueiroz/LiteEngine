#include <filament/data/assets/FilamentMeshAsset3dInstance.h>

#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>

namespace lite {

FilamentMeshAsset3dInstance::FilamentMeshAsset3dInstance(filament::Engine* engine, filament::Scene* scene)
    : m_engine(engine)
    , m_scene(scene)
{
}

void FilamentMeshAsset3dInstance::initializeTransform(utils::Entity entity) {
    m_entity = entity;
    auto& tm = m_engine->getTransformManager();
    m_transform = std::make_unique<FilamentAsset3dTransform>(tm, entity);
}

std::vector<glm::vec3> FilamentMeshAsset3dInstance::getVertex() const {
    return cpuPositions;
}

std::vector<int64_t> FilamentMeshAsset3dInstance::getIndex() const {
    return std::vector<int64_t>(cpuIndices.begin(), cpuIndices.end());
}

std::vector<glm::vec2> FilamentMeshAsset3dInstance::getUVS(int index) const {
    // No UV storage on this instance (only positions/normals/indices are kept CPU-side)
    (void)index;
    return {};
}

std::vector<glm::vec3> FilamentMeshAsset3dInstance::getBoundingBox() {
    if (!m_boundsSet) {
        setBoundingBox(calcBoundingBox());
    }
    return { boundsMin, boundsMax };
}

void FilamentMeshAsset3dInstance::setBoundingBox(const std::vector<glm::vec3>& bounds) {
    if (bounds.size() < 2) return;
    boundsMin = bounds[0];
    boundsMax = bounds[1];
    m_boundsSet = true;
}

std::vector<glm::vec3> FilamentMeshAsset3dInstance::calcBoundingBox() const {
    if (cpuPositions.empty()) {
        return { glm::vec3(0.0f), glm::vec3(0.0f) };
    }
    glm::vec3 mn = cpuPositions[0];
    glm::vec3 mx = cpuPositions[0];
    for (const glm::vec3& p : cpuPositions) {
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }
    return { mn, mx };
}

void FilamentMeshAsset3dInstance::setVisible(bool visible) {
    m_visible = visible;

    auto& renderableManager = m_engine->getRenderableManager();
    auto instance = renderableManager.getInstance(m_entity);
    if (instance) {
        // Use setLayerMask to show/hide
        // Layer 0 is typically the main camera layer
        renderableManager.setLayerMask(instance, 0xFF, visible ? 0xFF : 0x00);
    }
}

} // namespace lite
