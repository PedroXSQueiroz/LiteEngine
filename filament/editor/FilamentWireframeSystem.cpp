#include <filament/editor/FilamentWireframeSystem.h>

#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <utils/EntityManager.h>

#include <filament/scene/FilamentScene.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

namespace lite {

FilamentWireframeSystem::FilamentWireframeSystem(filament::Engine* engine, filament::Scene* scene)
    : m_engine(engine)
    , m_scene(scene)
{
}

FilamentWireframeSystem::~FilamentWireframeSystem() {
    clearWireframeMeshes();

    if (m_wireframeMaterialInstance) m_engine->destroy(m_wireframeMaterialInstance);
    if (m_wireframeMaterial) m_engine->destroy(m_wireframeMaterial);
}

void FilamentWireframeSystem::initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_initialized = true;

    std::cout << "FilamentWireframeSystem: Initialized " << width << "x" << height << std::endl;
}

void FilamentWireframeSystem::resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) return;

    m_width = width;
    m_height = height;
}

void FilamentWireframeSystem::loadMaterial(const std::string& materialPath) {
    std::cout << "FilamentWireframeSystem: Loading material from " << materialPath << std::endl;

    std::ifstream file(materialPath, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        file.read(buffer.data(), size);

        m_wireframeMaterial = filament::Material::Builder()
            .package(buffer.data(), buffer.size())
            .build(*m_engine);

        if (m_wireframeMaterial) {
            m_wireframeMaterialInstance = m_wireframeMaterial->createInstance();
            m_wireframeMaterialInstance->setParameter("wireColor",
                filament::math::float4{m_wireframeColor.r, m_wireframeColor.g,
                                       m_wireframeColor.b, m_wireframeColor.a});
            m_wireframeMaterialInstance->setParameter("wireWidth", m_wireframeWidth);
            std::cout << "FilamentWireframeSystem: Material loaded OK" << std::endl;
        } else {
            std::cerr << "FilamentWireframeSystem: Material build failed" << std::endl;
        }
    } else {
        std::cerr << "FilamentWireframeSystem: Failed to open " << materialPath << std::endl;
    }
}

void FilamentWireframeSystem::setWireframeColor(const glm::vec4& color) {
    m_wireframeColor = color;
    if (m_wireframeMaterialInstance) {
        m_wireframeMaterialInstance->setParameter("wireColor",
            filament::math::float4{color.r, color.g, color.b, color.a});
    }
}

void FilamentWireframeSystem::setWireframeWidth(float width) {
    m_wireframeWidth = width;
    if (m_wireframeMaterialInstance) {
        m_wireframeMaterialInstance->setParameter("wireWidth", width);
    }
}

void FilamentWireframeSystem::addWireframeMesh(FilamentMeshAsset3dInstance* mesh) {
    if (!mesh) return;
    if (!mesh->isMesh()) return;
    if (m_wireframeMeshes.count(mesh) > 0) return;

    m_wireframeMeshes.insert(mesh);
    updateWireframeEntities();
}

void FilamentWireframeSystem::removeWireframeMesh(FilamentMeshAsset3dInstance* mesh) {
    if (!mesh || m_wireframeMeshes.count(mesh) == 0) return;

    if(m_wireframeMeshes.contains(mesh))
        m_wireframeMeshes.erase(mesh);
    
    updateWireframeEntities();
}

void FilamentWireframeSystem::clearWireframeMeshes() {
    m_wireframeMeshes.clear();

    // Remove and destroy all wireframe entities
    for (auto& wireEntity : m_wireframeEntities) {
        m_scene->remove(wireEntity.entity);
        if (wireEntity.vertexBuffer) m_engine->destroy(wireEntity.vertexBuffer);
        if (wireEntity.indexBuffer) m_engine->destroy(wireEntity.indexBuffer);
        m_engine->destroy(wireEntity.entity);
    }
    m_wireframeEntities.clear();
}

void FilamentWireframeSystem::updateWireframeEntities() {
    // Clear old entities
    for (auto& wireEntity : m_wireframeEntities) {
        m_scene->remove(wireEntity.entity);
        if (wireEntity.vertexBuffer) m_engine->destroy(wireEntity.vertexBuffer);
        if (wireEntity.indexBuffer) m_engine->destroy(wireEntity.indexBuffer);
        m_engine->destroy(wireEntity.entity);
    }
    m_wireframeEntities.clear();

    if (!m_wireframeMaterialInstance) {
        std::cerr << "FilamentWireframeSystem: No material instance available" << std::endl;
        return;
    }

    auto& transformManager = m_engine->getTransformManager();

    // Create wireframe entities for each mesh
    for (auto* baseMesh : m_wireframeMeshes) {
        auto* mesh = dynamic_cast<FilamentMeshAsset3dInstance*>(baseMesh);
        if (!mesh) continue;

        // Check if we have CPU data
        if (mesh->cpuPositions.empty() || mesh->cpuIndices.empty()) {
            std::cerr << "FilamentWireframeSystem: Mesh has no CPU data for wireframe" << std::endl;
            continue;
        }

        // Create expanded vertex data with barycentric coordinates
        // Each triangle gets 3 unique vertices with barycentric coords (1,0,0), (0,1,0), (0,0,1)
        size_t triangleCount = mesh->cpuIndices.size() / 3;
        uint32_t expandedVertexCount = static_cast<uint32_t>(triangleCount * 3);

        // Allocate expanded data
        auto* positions = new std::vector<filament::math::float3>();
        positions->resize(expandedVertexCount);
        auto* barycentrics = new std::vector<filament::math::float4>();
        barycentrics->resize(expandedVertexCount);

        // Barycentric coordinates for each vertex of a triangle
        filament::math::float4 bary0 = {1.0f, 0.0f, 0.0f, 1.0f};
        filament::math::float4 bary1 = {0.0f, 1.0f, 0.0f, 1.0f};
        filament::math::float4 bary2 = {0.0f, 0.0f, 1.0f, 1.0f};

        // Expand triangles
        for (size_t t = 0; t < triangleCount; ++t) {
            uint32_t i0 = mesh->cpuIndices[t * 3 + 0];
            uint32_t i1 = mesh->cpuIndices[t * 3 + 1];
            uint32_t i2 = mesh->cpuIndices[t * 3 + 2];

            const glm::vec3& p0 = mesh->cpuPositions[i0];
            const glm::vec3& p1 = mesh->cpuPositions[i1];
            const glm::vec3& p2 = mesh->cpuPositions[i2];

            size_t baseIdx = t * 3;
            (*positions)[baseIdx + 0] = {p0.x, p0.y, p0.z};
            (*positions)[baseIdx + 1] = {p1.x, p1.y, p1.z};
            (*positions)[baseIdx + 2] = {p2.x, p2.y, p2.z};

            (*barycentrics)[baseIdx + 0] = bary0;
            (*barycentrics)[baseIdx + 1] = bary1;
            (*barycentrics)[baseIdx + 2] = bary2;
        }

        //FIXME: SERÁ QUE NÃO DEVERIA REAPROVEITAR OS BUFFERS DO MESH ORIGINAL?
        // Create vertex buffer with position and color (barycentric) attributes
        filament::VertexBuffer* vertexBuffer = filament::VertexBuffer::Builder()
            .vertexCount(expandedVertexCount)
            .bufferCount(2)
            .attribute(filament::VertexAttribute::POSITION, 0,
                       filament::VertexBuffer::AttributeType::FLOAT3, 0, 12)
            .attribute(filament::VertexAttribute::COLOR, 1,
                       filament::VertexBuffer::AttributeType::FLOAT4, 0, 16)
            .build(*m_engine);

        // Set position buffer
        vertexBuffer->setBufferAt(*m_engine, 0,
            filament::VertexBuffer::BufferDescriptor(
                positions->data(),
                positions->size() * sizeof(filament::math::float3),
                [](void* buffer, size_t size, void* user) {
                    delete static_cast<std::vector<filament::math::float3>*>(user);
                },
                positions
            )
        );

        // Set barycentric/color buffer
        vertexBuffer->setBufferAt(*m_engine, 1,
            filament::VertexBuffer::BufferDescriptor(
                barycentrics->data(),
                barycentrics->size() * sizeof(filament::math::float4),
                [](void* buffer, size_t size, void* user) {
                    delete static_cast<std::vector<filament::math::float4>*>(user);
                },
                barycentrics
            )
        );

        // Create index buffer (sequential indices since we expanded vertices)
        auto* indices = new std::vector<uint32_t>(expandedVertexCount);
        for (uint32_t i = 0; i < expandedVertexCount; ++i) {
            (*indices)[i] = i;
        }

        filament::IndexBuffer* indexBuffer = filament::IndexBuffer::Builder()
            .indexCount(expandedVertexCount)
            .bufferType(filament::IndexBuffer::IndexType::UINT)
            .build(*m_engine);

        indexBuffer->setBuffer(*m_engine,
            filament::IndexBuffer::BufferDescriptor(
                indices->data(),
                indices->size() * sizeof(uint32_t),
                [](void* buffer, size_t size, void* user) {
                    delete static_cast<std::vector<uint32_t>*>(user);
                },
                indices
            )
        );

        // Create entity
        utils::Entity entity = utils::EntityManager::get().create();

        // Build renderable
        filament::RenderableManager::Builder(1)
            .boundingBox({{mesh->boundsMin.x, mesh->boundsMin.y, mesh->boundsMin.z},
                          {mesh->boundsMax.x, mesh->boundsMax.y, mesh->boundsMax.z}})
            .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
                     vertexBuffer, indexBuffer, 0, expandedVertexCount)
            .material(0, m_wireframeMaterialInstance)
            .culling(false)
            .castShadows(false)
            .receiveShadows(false)
            .priority(7) // Render after main geometry
            .build(*m_engine, entity);

        // Copy transform from original mesh
        transformManager.create(entity);
        auto originalTransformInstance = transformManager.getInstance(mesh->getFilamentEntity());
        auto newTransformInstance = transformManager.getInstance(entity);
        if (originalTransformInstance && newTransformInstance) {
            transformManager.setTransform(newTransformInstance,
                transformManager.getWorldTransform(originalTransformInstance));
        }

        // Add to scene
        m_scene->addEntity(entity);

        // Store reference
        m_wireframeEntities.push_back({entity, mesh, vertexBuffer, indexBuffer});

        std::cout << "FilamentWireframeSystem: Created wireframe for mesh with "
                  << triangleCount << " triangles" << std::endl;
    }

    std::cout << "FilamentWireframeSystem: Updated " << m_wireframeEntities.size()
              << " wireframe entities" << std::endl;
}

void FilamentWireframeSystem::ensureGpuResources() {
    if (m_gpuReady || m_materialPath.empty()) return;

    loadMaterial(m_materialPath);
    m_gpuReady = (m_wireframeMaterialInstance != nullptr);

    if (!m_gpuReady) {
        // Falhou (path errado, arquivo corrompido): não tentar de novo a cada frame
        m_materialPath.clear();
    }
}

void FilamentWireframeSystem::forEachMesh(
    Asset3dInstance<FilamentAsset3dTransform>& node,
    const std::function<void(FilamentMeshAsset3dInstance*)>& fn
) {
    if (node.isMesh()) {
        if (auto* mesh = dynamic_cast<FilamentMeshAsset3dInstance*>(&node)) {
            fn(mesh);
        }
    }
    for (auto& child : node.children) {
        forEachMesh(*child, fn);
    }
}

// Fase onFrameBegin: roda logo após Scene::instantiate(), então assets criados
// neste frame já estão no mapa e ganham wireframe imediatamente.
void FilamentWireframeSystem::onFrameBegin(float) {
    if (!m_initialized || !m_liteScene) return;

    ensureGpuResources();

    if (!m_autoTrack || !m_wireframeMaterialInstance) return;

    bool changed = false;
    auto alive = m_liteScene->find([](FilamentAsset3dInstance* asset) {
        return !asset->isDeleted();
    });

    for (auto* asset : alive) {
        forEachMesh(*asset, [&](FilamentMeshAsset3dInstance* mesh) {
            if (!m_wireframeMeshes.contains(mesh)) {
                m_wireframeMeshes.insert(mesh);
                changed = true;
            }
        });
    }

    // Rebuild único por frame, não um por mesh (addWireframeMesh reconstruiria N vezes)
    if (changed) updateWireframeEntities();
}

// Fase onFrameEnd: roda após finishRender() — o flush já destruiu os recursos
// GPU dos assets deletados; as entidades wireframe têm buffers próprios e são
// destruídas aqui, no mesmo frame.
void FilamentWireframeSystem::onFrameEnd(float) {
    if (!m_liteScene || m_wireframeMeshes.empty()) return;

    bool changed = false;
    auto deleted = m_liteScene->find([](FilamentAsset3dInstance* asset) {
        return asset->isDeleted();
    });

    for (auto* asset : deleted) {
        forEachMesh(*asset, [&](FilamentMeshAsset3dInstance* mesh) {
            if (m_wireframeMeshes.erase(mesh) > 0) {
                changed = true;
            }
        });
    }

    if (changed) updateWireframeEntities();
}

void FilamentWireframeSystem::update() {
    if (!m_initialized || m_wireframeMeshes.empty()) return;

    // Update transforms of wireframe entities to match source meshes
    auto& transformManager = m_engine->getTransformManager();

    for (auto& wireEntity : m_wireframeEntities) {
        if (!wireEntity.sourceMesh) continue;

        auto sourceTransformInstance = transformManager.getInstance(wireEntity.sourceMesh->getFilamentEntity() );
        auto wireTransformInstance = transformManager.getInstance(wireEntity.entity);

        if (sourceTransformInstance && wireTransformInstance) {
            transformManager.setTransform(wireTransformInstance,
                transformManager.getWorldTransform(sourceTransformInstance));
        }
    }
}

} // namespace lite
