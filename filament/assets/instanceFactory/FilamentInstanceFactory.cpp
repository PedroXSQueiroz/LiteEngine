#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>

#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <filament/TextureSampler.h>
#include <utils/EntityManager.h>
#include <imageio/ImageDecoder.h>
#include <image/LinearImage.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <functional>

namespace lite {

FilamentInstanceFactory::FilamentInstanceFactory(
    filament::Engine* engine,
    filament::Scene* scene,
    const std::string& defaultMaterialPath
)
    : m_engine(engine)
    , m_scene(scene)
    , m_defaultMaterialPath(defaultMaterialPath)
{
    if (m_defaultMaterialPath.empty()) {
        m_defaultMaterialPath = "D:/Workspace/LiteEngine/core/resources/filament/materials/lit.filamat";
    }

    // Load base material
    std::ifstream file(m_defaultMaterialPath, std::ios::binary);
    if (file.is_open()) {
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
        file.close();

        m_baseMaterial = filament::Material::Builder()
            .package(data.data(), data.size())
            .build(*m_engine);

        if (m_baseMaterial) {
            std::cout << "FilamentInstanceFactory: Base material loaded" << std::endl;
        }
    } else {
        std::cerr << "FilamentInstanceFactory: Failed to load base material: " << m_defaultMaterialPath << std::endl;
    }
}

FilamentInstanceFactory::~FilamentInstanceFactory() {
    cleanup();
}

void FilamentInstanceFactory::cleanup() {
    // Cleanup texture cache
    for (auto& [path, texture] : m_textureCache) {
        if (texture) {
            m_engine->destroy(texture);
        }
    }
    m_textureCache.clear();

    // Cleanup base material
    if (m_baseMaterial) {
        m_engine->destroy(m_baseMaterial);
        m_baseMaterial = nullptr;
    }
}

std::unique_ptr<Asset3dInstance> FilamentInstanceFactory::instantiate(
    const Asset3dData& rootNode,
    const std::vector<MaterialData>& materials
) {
    if (!m_baseMaterial) {
        std::cerr << "FilamentInstanceFactory: No base material available" << std::endl;
        return nullptr;
    }

    auto instance = std::make_unique<FilamentAsset3dInstance>(m_engine, m_scene);
    instance->name = rootNode.name;

    // Create root entity and transform
    utils::Entity rootEntity = utils::EntityManager::get().create();
    auto& transformManager = m_engine->getTransformManager();
    transformManager.create(rootEntity);
    instance->initializeTransform(rootEntity);
    instance->setLocalMatrix(rootNode.localTransform);

    // Create material instances and build lookup map by name
    std::unordered_map<std::string, filament::MaterialInstance*> materialMap;
    for (const auto& matData : materials) {
        filament::MaterialInstance* matInstance = createMaterialInstance(matData);
        materialMap[matData.name] = matInstance;
        instance->materialInstances.push_back(matInstance);
    }

    // Process node hierarchy recursively - creates FilamentMeshAsset3dInstance for meshes
    processNode(rootNode, *instance, materialMap);

    // Count total meshes created
    size_t meshCount = 0;
    std::function<void(const Asset3dInstance&)> countMeshes = [&](const Asset3dInstance& node) {
        if (node.isMesh()) meshCount++;
        for (const auto& child : node.children) {
            countMeshes(*child);
        }
    };
    countMeshes(*instance);

    std::cout << "FilamentInstanceFactory: Created instance with "
              << meshCount << " meshes" << std::endl;

    return instance;
}

void FilamentInstanceFactory::destroy(Asset3dInstance* instance) {
    if (!instance) return;

    auto* filamentInstance = dynamic_cast<FilamentAsset3dInstance*>(instance);
    if (!filamentInstance) return;

    auto& transformManager = m_engine->getTransformManager();

    // Recursively destroy resources in hierarchy
    std::function<void(Asset3dInstance&)> destroyNode = [&](Asset3dInstance& node) {
        // First destroy children
        for (auto& child : node.children) {
            destroyNode(*child);
        }

        // If this is a mesh, destroy its GPU resources
        if (node.isMesh()) {
            auto* meshInstance = dynamic_cast<FilamentMeshAsset3dInstance*>(&node);
            if (meshInstance) {
                utils::Entity entity = meshInstance->getEntity();

                // Remove from scene
                m_scene->remove(entity);

                // Destroy GPU resources
                if (meshInstance->vertexBuffer) {
                    m_engine->destroy(meshInstance->vertexBuffer);
                }
                if (meshInstance->indexBuffer) {
                    m_engine->destroy(meshInstance->indexBuffer);
                }
                // Note: materialInstance is shared, destroyed below

                // Destroy transform and entity
                transformManager.destroy(entity);
                m_engine->destroy(entity);
            }
        } else {
            // Non-mesh node (FilamentAsset3dInstance or intermediate)
            auto* filamentNode = dynamic_cast<FilamentAsset3dInstance*>(&node);
            if (filamentNode) {
                utils::Entity entity = filamentNode->getEntity();
                transformManager.destroy(entity);
                m_engine->destroy(entity);
            }
        }
    };
    destroyNode(*filamentInstance);

    // Destroy shared material instances
    for (auto* mi : filamentInstance->materialInstances) {
        if (mi) m_engine->destroy(mi);
    }

    std::cout << "FilamentInstanceFactory: Instance destroyed" << std::endl;
}

void FilamentInstanceFactory::processNode(
    const Asset3dData& node,
    Asset3dInstance& parentInstance,
    const std::unordered_map<std::string, filament::MaterialInstance*>& materialMap
) {
    auto& transformManager = m_engine->getTransformManager();

    // Get root instance for accessing shared materials
    FilamentAsset3dInstance* rootInstance = nullptr;
    Asset3dInstance* current = &parentInstance;
    while (current) {
        if (auto* root = dynamic_cast<FilamentAsset3dInstance*>(current)) {
            rootInstance = root;
            break;
        }
        current = current->parent;
    }

    // Get parent entity for transform hierarchy
    utils::Entity parentEntity;
    if (auto* filamentParent = dynamic_cast<FilamentAsset3dInstance*>(&parentInstance)) {
        parentEntity = filamentParent->getEntity();
    } else if (auto* meshParent = dynamic_cast<FilamentMeshAsset3dInstance*>(&parentInstance)) {
        parentEntity = meshParent->getEntity();
    }

    // If this is a mesh node, create FilamentMeshAsset3dInstance
    if (node.isMesh()) {
        const MeshAsset3dData& mesh = static_cast<const MeshAsset3dData&>(node);

        // Create mesh instance as child
        auto* meshInstance = parentInstance.addChild<FilamentMeshAsset3dInstance>(m_engine, m_scene);
        meshInstance->name = mesh.name;
        meshInstance->materialName = mesh.materialName;

        // Create vertex buffer
        meshInstance->vertexBuffer = createVertexBuffer(mesh);
        if (!meshInstance->vertexBuffer) return;

        // Create index buffer
        meshInstance->indexBuffer = createIndexBuffer(mesh);
        if (!meshInstance->indexBuffer) return;

        // Get material instance by name
        auto it = materialMap.find(mesh.materialName);
        if (it != materialMap.end()) {
            meshInstance->materialInstance = it->second;
        } else if (rootInstance && !rootInstance->materialInstances.empty()) {
            // Fallback to first material
            meshInstance->materialInstance = rootInstance->materialInstances[0];
        } else {
            // Create default material instance
            meshInstance->materialInstance = m_baseMaterial->createInstance();
            if (rootInstance) {
                rootInstance->materialInstances.push_back(meshInstance->materialInstance);
            }
        }

        // Create entity
        utils::Entity meshEntity = utils::EntityManager::get().create();

        // Build renderable
        filament::RenderableManager::Builder(1)
            .boundingBox({{toFilament(mesh.boundsMin)}, {toFilament(mesh.boundsMax)}})
            .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
                      meshInstance->vertexBuffer, meshInstance->indexBuffer, 0, mesh.indices.size())
            .material(0, meshInstance->materialInstance)
            .culling(false)
            .castShadows(false)
            .receiveShadows(true)
            .build(*m_engine, meshEntity);

        // Create transform with parent relationship
        auto parentTransformInstance = transformManager.getInstance(parentEntity);
        transformManager.create(meshEntity, parentTransformInstance, toFilament(mesh.localTransform));

        // Initialize the transform wrapper
        meshInstance->initializeTransform(meshEntity);

        // Add to scene
        m_scene->addEntity(meshEntity);

        // Process children of this mesh node (if any)
        for (const auto& child : node.children) {
            processNode(*child, *meshInstance, materialMap);
        }
    } else {
        // Non-mesh node - create intermediate FilamentAsset3dInstance to maintain hierarchy
        // Only create if this node has children or a non-identity transform
        if (!node.children.empty() || node.localTransform != glm::mat4(1.0f)) {
            auto* intermediateInstance = parentInstance.addChild<FilamentAsset3dInstance>(m_engine, m_scene);
            intermediateInstance->name = node.name;

            // Create entity for transform hierarchy
            utils::Entity intermediateEntity = utils::EntityManager::get().create();
            auto parentTransformInstance = transformManager.getInstance(parentEntity);
            transformManager.create(intermediateEntity, parentTransformInstance, toFilament(node.localTransform));
            intermediateInstance->initializeTransform(intermediateEntity);

            // Process children recursively
            for (const auto& child : node.children) {
                processNode(*child, *intermediateInstance, materialMap);
            }
        } else {
            // Skip this empty node, process children directly on parent
            for (const auto& child : node.children) {
                processNode(*child, parentInstance, materialMap);
            }
        }
    }
}

filament::VertexBuffer* FilamentInstanceFactory::createVertexBuffer(const MeshAsset3dData& mesh) {
    if (mesh.positions.empty()) return nullptr;

    // Allocate persistent copies of data
    auto* positions = new std::vector<filament::math::float3>(mesh.positions.size());
    auto* normals = new std::vector<filament::math::float3>(mesh.normals.size());
    auto* uvs = new std::vector<filament::math::float2>(mesh.uvs.size());

    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        (*positions)[i] = toFilament(mesh.positions[i]);
    }
    for (size_t i = 0; i < mesh.normals.size(); ++i) {
        (*normals)[i] = toFilament(mesh.normals[i]);
    }
    for (size_t i = 0; i < mesh.uvs.size(); ++i) {
        (*uvs)[i] = {mesh.uvs[i].x, mesh.uvs[i].y};
    }

    filament::VertexBuffer* vertexBuffer = filament::VertexBuffer::Builder()
        .vertexCount(mesh.positions.size())
        .bufferCount(3)
        .attribute(filament::VertexAttribute::POSITION, 0,
                   filament::VertexBuffer::AttributeType::FLOAT3, 0, 12)
        .attribute(filament::VertexAttribute::TANGENTS, 1,
                   filament::VertexBuffer::AttributeType::FLOAT3, 0, 12)
        .attribute(filament::VertexAttribute::UV0, 2,
                   filament::VertexBuffer::AttributeType::FLOAT2, 0, 8)
        .build(*m_engine);

    // Set buffers with cleanup callbacks
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

    vertexBuffer->setBufferAt(*m_engine, 1,
        filament::VertexBuffer::BufferDescriptor(
            normals->data(),
            normals->size() * sizeof(filament::math::float3),
            [](void* buffer, size_t size, void* user) {
                delete static_cast<std::vector<filament::math::float3>*>(user);
            },
            normals
        )
    );

    vertexBuffer->setBufferAt(*m_engine, 2,
        filament::VertexBuffer::BufferDescriptor(
            uvs->data(),
            uvs->size() * sizeof(filament::math::float2),
            [](void* buffer, size_t size, void* user) {
                delete static_cast<std::vector<filament::math::float2>*>(user);
            },
            uvs
        )
    );

    return vertexBuffer;
}

filament::IndexBuffer* FilamentInstanceFactory::createIndexBuffer(const MeshAsset3dData& mesh) {
    if (mesh.indices.empty()) return nullptr;

    auto* indices = new std::vector<uint32_t>(mesh.indices);

    filament::IndexBuffer* indexBuffer = filament::IndexBuffer::Builder()
        .indexCount(indices->size())
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

    return indexBuffer;
}

filament::Texture* FilamentInstanceFactory::loadTexture(const TextureInfo& texInfo) {
    if (texInfo.path.empty()) return nullptr;

    // Check cache
    auto it = m_textureCache.find(texInfo.path);
    if (it != m_textureCache.end()) {
        return it->second;
    }

    std::cout << "  Loading texture: " << texInfo.path << std::endl;

    std::ifstream file(texInfo.path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "  Failed to open texture: " << texInfo.path << std::endl;
        return nullptr;
    }

    image::ImageDecoder::ColorSpace colorSpace = texInfo.sRGB
        ? image::ImageDecoder::ColorSpace::SRGB
        : image::ImageDecoder::ColorSpace::LINEAR;

    image::LinearImage image = image::ImageDecoder::decode(file, texInfo.path, colorSpace);
    file.close();

    uint32_t width = image.getWidth();
    uint32_t height = image.getHeight();
    uint32_t channels = image.getChannels();

    if (width == 0 || height == 0) {
        std::cerr << "  Failed to decode texture: " << texInfo.path << std::endl;
        return nullptr;
    }

    filament::Texture::InternalFormat format;
    if (texInfo.sRGB) {
        format = (channels == 4)
            ? filament::Texture::InternalFormat::SRGB8_A8
            : filament::Texture::InternalFormat::SRGB8;
    } else {
        format = (channels == 4)
            ? filament::Texture::InternalFormat::RGBA8
            : filament::Texture::InternalFormat::RGB8;
    }

    filament::Texture* texture = filament::Texture::Builder()
        .width(width)
        .height(height)
        .levels(1)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .format(format)
        .build(*m_engine);

    size_t dataSize = width * height * channels;
    void* pixelDataCopy = malloc(dataSize);
    memcpy(pixelDataCopy, image.getPixelRef(), dataSize);

    filament::Texture::PixelBufferDescriptor buffer(
        pixelDataCopy,
        dataSize,
        (channels == 4) ? filament::Texture::Format::RGBA : filament::Texture::Format::RGB,
        filament::Texture::Type::UBYTE,
        [](void* buffer, size_t size, void* user) {
            free(buffer);
        },
        nullptr
    );

    texture->setImage(*m_engine, 0, std::move(buffer));

    m_textureCache[texInfo.path] = texture;

    std::cout << "  Loaded: " << width << "x" << height << " (" << channels << " ch)" << std::endl;

    return texture;
}

filament::MaterialInstance* FilamentInstanceFactory::createMaterialInstance(const MaterialData& matData) {
    if (!m_baseMaterial) return nullptr;

    filament::MaterialInstance* instance = m_baseMaterial->createInstance();

    filament::TextureSampler sampler(
        filament::TextureSampler::MinFilter::LINEAR_MIPMAP_LINEAR,
        filament::TextureSampler::MagFilter::LINEAR,
        filament::TextureSampler::WrapMode::REPEAT
    );

    // Base color
    instance->setParameter("baseColorFactor", toFilament(matData.baseColorFactor));
    if (matData.baseColorTexture.has_value()) {
        filament::Texture* tex = loadTexture(matData.baseColorTexture.value());
        if (tex) {
            instance->setParameter("baseColorMap", tex, sampler);
        }
    }

    // Metallic/Roughness
    instance->setParameter("metallicFactor", matData.metallicFactor);
    instance->setParameter("roughnessFactor", matData.roughnessFactor);
    if (matData.metallicRoughnessTexture.has_value()) {
        filament::Texture* tex = loadTexture(matData.metallicRoughnessTexture.value());
        if (tex) {
            instance->setParameter("metallicRoughnessMap", tex, sampler);
        }
    }

    // Normal
    if (matData.normalTexture.has_value()) {
        filament::Texture* tex = loadTexture(matData.normalTexture.value());
        if (tex) {
            instance->setParameter("normalMap", tex, sampler);
        }
    }

    // Occlusion
    if (matData.occlusionTexture.has_value()) {
        filament::Texture* tex = loadTexture(matData.occlusionTexture.value());
        if (tex) {
            instance->setParameter("occlusionMap", tex, sampler);
        }
    }

    // Emissive
    instance->setParameter("emissiveFactor", toFilament(matData.emissiveFactor));
    if (matData.emissiveTexture.has_value()) {
        filament::Texture* tex = loadTexture(matData.emissiveTexture.value());
        if (tex) {
            instance->setParameter("emissiveMap", tex, sampler);
        }
    }

    return instance;
}

// Type conversion helpers
filament::math::float3 FilamentInstanceFactory::toFilament(const glm::vec3& v) {
    return {v.x, v.y, v.z};
}

filament::math::float4 FilamentInstanceFactory::toFilament(const glm::vec4& v) {
    return {v.x, v.y, v.z, v.w};
}

filament::math::mat4f FilamentInstanceFactory::toFilament(const glm::mat4& m) {
    filament::math::mat4f result;
    std::memcpy(&result, &m, sizeof(float) * 16);
    return result;
}

} // namespace lite
