#include <lite/renderers/filament/FilamentInstantiator.h>

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

namespace lite {

FilamentInstantiator::FilamentInstantiator(
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
            std::cout << "FilamentInstantiator: Base material loaded" << std::endl;
        }
    } else {
        std::cerr << "FilamentInstantiator: Failed to load base material: " << m_defaultMaterialPath << std::endl;
    }
}

FilamentInstantiator::~FilamentInstantiator() {
    cleanup();
}

void FilamentInstantiator::cleanup() {
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

std::unique_ptr<Asset3dInstance> FilamentInstantiator::instantiate(const Asset3dImportData& data) {
    if (!m_baseMaterial) {
        std::cerr << "FilamentInstantiator: No base material available" << std::endl;
        return nullptr;
    }

    auto instance = std::make_unique<FilamentAsset3dInstance>(m_engine, m_scene);

    // Create material instances for each material
    for (const auto& matData : data.materials) {
        filament::MaterialInstance* matInstance = createMaterialInstance(matData);
        instance->materialInstances.push_back(matInstance);
    }

    // Create root entity
    instance->rootEntity = utils::EntityManager::get().create();
    auto& transformManager = m_engine->getTransformManager();
    transformManager.create(instance->rootEntity);

    // Process scene hierarchy starting from root
    if (!data.nodes.empty()) {
        processNode(data, data.rootNodeIndex, *instance, glm::mat4(1.0f));
    }

    std::cout << "FilamentInstantiator: Created instance with "
              << instance->entities.size() << " entities" << std::endl;

    return instance;
}

void FilamentInstantiator::destroy(Asset3dInstance* instance) {
    if (!instance) return;

    auto* filamentInstance = dynamic_cast<FilamentAsset3dInstance*>(instance);
    if (!filamentInstance) return;

    // Remove entities from scene and destroy
    for (auto& entity : filamentInstance->entities) {
        m_scene->remove(entity);
        m_engine->destroy(entity);
    }

    // Destroy vertex buffers
    for (auto* vb : filamentInstance->vertexBuffers) {
        if (vb) m_engine->destroy(vb);
    }

    // Destroy index buffers
    for (auto* ib : filamentInstance->indexBuffers) {
        if (ib) m_engine->destroy(ib);
    }

    // Destroy material instances
    for (auto* mi : filamentInstance->materialInstances) {
        if (mi) m_engine->destroy(mi);
    }

    // Destroy root entity
    m_engine->destroy(filamentInstance->rootEntity);

    std::cout << "FilamentInstantiator: Instance destroyed" << std::endl;
}

void FilamentInstantiator::processNode(
    const Asset3dImportData& data,
    uint32_t nodeIndex,
    FilamentAsset3dInstance& instance,
    const glm::mat4& parentTransform
) {
    if (nodeIndex >= data.nodes.size()) return;

    const SceneNode& node = data.nodes[nodeIndex];
    glm::mat4 worldTransform = parentTransform * node.localTransform;

    // Create entities for each mesh in this node
    for (uint32_t meshIndex : node.meshIndices) {
        if (meshIndex >= data.meshes.size()) continue;

        const MeshImportData& mesh = data.meshes[meshIndex];

        // Create vertex buffer
        filament::VertexBuffer* vertexBuffer = createVertexBuffer(mesh);
        if (!vertexBuffer) continue;
        instance.vertexBuffers.push_back(vertexBuffer);

        // Create index buffer
        filament::IndexBuffer* indexBuffer = createIndexBuffer(mesh);
        if (!indexBuffer) continue;
        instance.indexBuffers.push_back(indexBuffer);

        // Get material instance
        filament::MaterialInstance* matInstance = nullptr;
        if (mesh.materialIndex >= 0 && mesh.materialIndex < static_cast<int32_t>(instance.materialInstances.size())) {
            matInstance = instance.materialInstances[mesh.materialIndex];
        } else if (!instance.materialInstances.empty()) {
            matInstance = instance.materialInstances[0];
        } else {
            matInstance = m_baseMaterial->createInstance();
            instance.materialInstances.push_back(matInstance);
        }

        // Create entity
        utils::Entity entity = utils::EntityManager::get().create();

        // Build renderable
        filament::RenderableManager::Builder(1)
            .boundingBox({{toFilament(mesh.boundsMin)}, {toFilament(mesh.boundsMax)}})
            .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
                      vertexBuffer, indexBuffer, 0, mesh.indices.size())
            .material(0, matInstance)
            .culling(false)
            .castShadows(false)
            .receiveShadows(true)
            .build(*m_engine, entity);

        // Set transform
        auto& transformManager = m_engine->getTransformManager();
        transformManager.create(entity);
        auto transformInstance = transformManager.getInstance(entity);
        if (transformInstance) {
            transformManager.setTransform(transformInstance, toFilament(worldTransform));
        }

        // Add to scene
        m_scene->addEntity(entity);
        instance.entities.push_back(entity);
    }

    // Process children
    for (uint32_t childIndex : node.childIndices) {
        processNode(data, childIndex, instance, worldTransform);
    }
}

filament::VertexBuffer* FilamentInstantiator::createVertexBuffer(const MeshImportData& mesh) {
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

filament::IndexBuffer* FilamentInstantiator::createIndexBuffer(const MeshImportData& mesh) {
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

filament::Texture* FilamentInstantiator::loadTexture(const TextureInfo& texInfo) {
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

filament::MaterialInstance* FilamentInstantiator::createMaterialInstance(const MaterialImportData& matData) {
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
filament::math::float3 FilamentInstantiator::toFilament(const glm::vec3& v) {
    return {v.x, v.y, v.z};
}

filament::math::float4 FilamentInstantiator::toFilament(const glm::vec4& v) {
    return {v.x, v.y, v.z, v.w};
}

filament::math::mat4f FilamentInstantiator::toFilament(const glm::mat4& m) {
    filament::math::mat4f result;
    std::memcpy(&result, &m, sizeof(float) * 16);
    return result;
}

} // namespace lite
