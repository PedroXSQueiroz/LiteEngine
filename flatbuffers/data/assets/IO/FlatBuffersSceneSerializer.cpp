#include <flatbuffers/data/assets/IO/FlatBuffersSceneSerializer.h>

#include <core/data/DTOs/CameraAsset3dInstanceDTO.h>
#include <core/data/DTOs/MeshAsset3dInstanceDTO.h>

#include <flatbuffers/data/assets/IO/scene_generated.h>

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <vector>
#include <cstdint>

namespace lite {

namespace {

namespace fbs = lite::serialization;

// Geração de formato que esta build sabe ler. Arquivo mais novo que isto é
// recusado — os campos que ele traria não existem aqui.
constexpr uint32_t SUPPORTED_SCHEMA_VERSION = 1;

// --------------------------------------------------------------------------
// Conversões de tipo de valor (glm <-> structs do schema)
// --------------------------------------------------------------------------

fbs::Vec2 toFb(const glm::vec2& v) { return fbs::Vec2(v.x, v.y); }
fbs::Vec3 toFb(const glm::vec3& v) { return fbs::Vec3(v.x, v.y, v.z); }
fbs::Vec4 toFb(const glm::vec4& v) { return fbs::Vec4(v.x, v.y, v.z, v.w); }

fbs::Mat4 toFb(const glm::mat4& m) {
    // glm é column-major e o Mat4 do schema guarda os 16 floats na mesma ordem.
    return fbs::Mat4(::flatbuffers::span<const float, 16>(glm::value_ptr(m), 16));
}

glm::vec2 toGlm(const fbs::Vec2* v) { return v ? glm::vec2(v->x(), v->y()) : glm::vec2(0.0f); }
glm::vec3 toGlm(const fbs::Vec3* v) { return v ? glm::vec3(v->x(), v->y(), v->z()) : glm::vec3(0.0f); }
glm::vec4 toGlm(const fbs::Vec4* v) { return v ? glm::vec4(v->x(), v->y(), v->z(), v->w()) : glm::vec4(1.0f); }

glm::mat4 toGlm(const fbs::Mat4* m) {
    if (!m) return glm::mat4(1.0f);
    return glm::make_mat4(m->m()->data());
}

// --------------------------------------------------------------------------
// SAVE: DTO -> FlatBuffers
// --------------------------------------------------------------------------

::flatbuffers::Offset<fbs::TextureInfoFB> buildTexture(
    ::flatbuffers::FlatBufferBuilder& fbb,
    const std::optional<TextureInfoDTO>& texture
) {
    if (!texture.has_value()) return 0;

    return fbs::CreateTextureInfoFBDirect(
        fbb,
        texture->path.c_str(),
        texture->sRGB,
        texture->width,
        texture->height,
        texture->channels
    );
}

// Devolve o offset da table concreta e escreve em outType qual variante da
// union MaterialKind foi construída.
::flatbuffers::Offset<void> buildMaterial(
    ::flatbuffers::FlatBufferBuilder& fbb,
    const MaterialDTO& material,
    uint8_t& outType
) {
    if (const auto* pbr = dynamic_cast<const MPBRLitMaterialDTO*>(&material)) {
        // Tudo que é table aninhada nasce ANTES da table que a contém: o
        // FlatBuffers não permite abrir uma table dentro de outra.
        auto base = fbs::CreateMaterialFBDirect(fbb, pbr->name.c_str());

        auto baseColorTexture        = buildTexture(fbb, pbr->baseColorTexture);
        auto normalTexture           = buildTexture(fbb, pbr->normalTexture);
        auto metallicRoughnessTexture= buildTexture(fbb, pbr->metallicRoughnessTexture);
        auto occlusionTexture        = buildTexture(fbb, pbr->occlusionTexture);
        auto emissiveTexture         = buildTexture(fbb, pbr->emissiveTexture);

        const fbs::Vec4 baseColorFactor = toFb(pbr->baseColorFactor);
        const fbs::Vec3 emissiveFactor  = toFb(pbr->emissiveFactor);

        outType = fbs::MaterialKind_MPBRLitMaterialFB;
        return fbs::CreateMPBRLitMaterialFB(
            fbb,
            base,
            &baseColorFactor,
            pbr->metallicFactor,
            pbr->roughnessFactor,
            &emissiveFactor,
            baseColorTexture,
            normalTexture,
            metallicRoughnessTexture,
            occlusionTexture,
            emissiveTexture
        ).Union();
    }

    outType = fbs::MaterialKind_MaterialFB;
    return fbs::CreateMaterialFBDirect(fbb, material.name.c_str()).Union();
}

// Idem para NodeKind. Recursiva: os filhos são construídos antes do pai.
::flatbuffers::Offset<void> buildNode(
    ::flatbuffers::FlatBufferBuilder& fbb,
    const Asset3dInstanceDTO& node,
    uint8_t& outType
) {
    std::vector<uint8_t> childrenTypes;
    std::vector<::flatbuffers::Offset<void>> children;
    childrenTypes.reserve(node.children.size());
    children.reserve(node.children.size());

    for (const auto& child : node.children) {
        if (!child) continue;
        uint8_t childType = fbs::NodeKind_NONE;
        auto childOffset = buildNode(fbb, *child, childType);
        childrenTypes.push_back(childType);
        children.push_back(childOffset);
    }

    const fbs::Mat4 localTransform = toFb(node.localTransform);

    auto base = fbs::CreateNodeFBDirect(
        fbb,
        node.id,
        node.name.c_str(),
        &localTransform,
        node.visible,
        &childrenTypes,
        &children
    );

    if (const auto* mesh = dynamic_cast<const MeshAsset3dInstanceDTO*>(&node)) {
        std::vector<fbs::Vec3> positions;
        positions.reserve(mesh->positions.size());
        for (const glm::vec3& p : mesh->positions) positions.push_back(toFb(p));

        std::vector<fbs::Vec3> normals;
        normals.reserve(mesh->normals.size());
        for (const glm::vec3& n : mesh->normals) normals.push_back(toFb(n));

        std::vector<fbs::Vec2> uvs;
        uvs.reserve(mesh->uvs.size());
        for (const glm::vec2& uv : mesh->uvs) uvs.push_back(toFb(uv));

        const fbs::Vec3 boundsMin = toFb(mesh->boundsMin);
        const fbs::Vec3 boundsMax = toFb(mesh->boundsMax);

        outType = fbs::NodeKind_MeshNodeFB;
        return fbs::CreateMeshNodeFBDirect(
            fbb,
            base,
            &positions,
            &normals,
            &uvs,
            &mesh->indices,
            &boundsMin,
            &boundsMax,
            mesh->materialName.c_str()
        ).Union();
    }

    if (const auto* camera = dynamic_cast<const CameraAsset3dInstanceDTO*>(&node)) {
        const fbs::Vec3 eye    = toFb(camera->eye);
        const fbs::Vec3 target = toFb(camera->target);

        outType = fbs::NodeKind_CameraNodeFB;
        return fbs::CreateCameraNodeFB(fbb, base, &eye, &target).Union();
    }

    outType = fbs::NodeKind_NodeFB;
    return base.Union();
}

// --------------------------------------------------------------------------
// LOAD: FlatBuffers -> DTO
// --------------------------------------------------------------------------

std::optional<TextureInfoDTO> readTexture(const fbs::TextureInfoFB* texture) {
    if (!texture) return std::nullopt;

    TextureInfoDTO dto;
    if (texture->path()) dto.path = texture->path()->str();
    dto.sRGB     = texture->srgb();
    dto.width    = texture->width();
    dto.height   = texture->height();
    dto.channels = texture->channels();
    return dto;
}

// nullptr = variante desconhecida (arquivo de uma geração que esta build não
// conhece); o chamador simplesmente pula a entrada.
std::unique_ptr<MaterialDTO> readMaterial(uint8_t type, const void* table) {
    if (!table) return nullptr;

    switch (type) {
        case fbs::MaterialKind_MPBRLitMaterialFB: {
            const auto* pbr = static_cast<const fbs::MPBRLitMaterialFB*>(table);
            auto dto = std::make_unique<MPBRLitMaterialDTO>();

            if (pbr->base() && pbr->base()->name()) dto->name = pbr->base()->name()->str();

            dto->baseColorFactor = toGlm(pbr->base_color_factor());
            dto->metallicFactor  = pbr->metallic_factor();
            dto->roughnessFactor = pbr->roughness_factor();
            dto->emissiveFactor  = toGlm(pbr->emissive_factor());

            dto->baseColorTexture         = readTexture(pbr->base_color_texture());
            dto->normalTexture            = readTexture(pbr->normal_texture());
            dto->metallicRoughnessTexture = readTexture(pbr->metallic_roughness_texture());
            dto->occlusionTexture         = readTexture(pbr->occlusion_texture());
            dto->emissiveTexture          = readTexture(pbr->emissive_texture());

            return dto;
        }
        case fbs::MaterialKind_MaterialFB: {
            const auto* material = static_cast<const fbs::MaterialFB*>(table);
            auto dto = std::make_unique<MaterialDTO>();
            if (material->name()) dto->name = material->name()->str();
            return dto;
        }
        default:
            return nullptr;
    }
}

std::unique_ptr<Asset3dInstanceDTO> readNode(uint8_t type, const void* table);

// Preenche a parte comum e desce nos filhos.
void readNodeBase(const fbs::NodeFB* base, Asset3dInstanceDTO& dto) {
    if (!base) return;

    dto.id = base->id();
    if (base->name()) dto.name = base->name()->str();
    dto.localTransform = toGlm(base->local_transform());
    dto.visible        = base->visible();

    const auto* childrenTypes = base->children_type();
    const auto* children      = base->children();
    if (!childrenTypes || !children) return;

    const size_t count = std::min<size_t>(childrenTypes->size(), children->size());
    for (size_t i = 0; i < count; ++i) {
        auto child = readNode(childrenTypes->Get(static_cast<uint32_t>(i)),
                              children->Get(static_cast<uint32_t>(i)));
        if (child) dto.children.push_back(std::move(child));
    }
}

std::unique_ptr<Asset3dInstanceDTO> readNode(uint8_t type, const void* table) {
    if (!table) return nullptr;

    switch (type) {
        case fbs::NodeKind_MeshNodeFB: {
            const auto* mesh = static_cast<const fbs::MeshNodeFB*>(table);
            auto dto = std::make_unique<MeshAsset3dInstanceDTO>();
            readNodeBase(mesh->base(), *dto);

            if (const auto* positions = mesh->positions()) {
                dto->positions.reserve(positions->size());
                for (const auto* p : *positions) dto->positions.push_back(toGlm(p));
            }
            if (const auto* normals = mesh->normals()) {
                dto->normals.reserve(normals->size());
                for (const auto* n : *normals) dto->normals.push_back(toGlm(n));
            }
            if (const auto* uvs = mesh->uvs()) {
                dto->uvs.reserve(uvs->size());
                for (const auto* uv : *uvs) dto->uvs.push_back(toGlm(uv));
            }
            if (const auto* indices = mesh->indices()) {
                dto->indices.assign(indices->begin(), indices->end());
            }

            dto->boundsMin = toGlm(mesh->bounds_min());
            dto->boundsMax = toGlm(mesh->bounds_max());
            if (mesh->material_name()) dto->materialName = mesh->material_name()->str();

            return dto;
        }
        case fbs::NodeKind_CameraNodeFB: {
            const auto* camera = static_cast<const fbs::CameraNodeFB*>(table);
            auto dto = std::make_unique<CameraAsset3dInstanceDTO>();
            readNodeBase(camera->base(), *dto);
            dto->eye    = toGlm(camera->eye());
            dto->target = toGlm(camera->target());
            return dto;
        }
        case fbs::NodeKind_NodeFB: {
            auto dto = std::make_unique<Asset3dInstanceDTO>();
            readNodeBase(static_cast<const fbs::NodeFB*>(table), *dto);
            return dto;
        }
        default:
            return nullptr;
    }
}

} // namespace

bool FlatBuffersSceneSerializer::save(const SceneDTO& scene, const std::string& path) {
    ::flatbuffers::FlatBufferBuilder fbb;

    std::vector<uint8_t> materialTypes;
    std::vector<::flatbuffers::Offset<void>> materials;
    materialTypes.reserve(scene.materials.size());
    materials.reserve(scene.materials.size());

    for (const auto& material : scene.materials) {
        if (!material) continue;
        uint8_t materialType = fbs::MaterialKind_NONE;
        auto offset = buildMaterial(fbb, *material, materialType);
        materialTypes.push_back(materialType);
        materials.push_back(offset);
    }

    std::vector<uint8_t> instanceTypes;
    std::vector<::flatbuffers::Offset<void>> instances;
    instanceTypes.reserve(scene.instances.size());
    instances.reserve(scene.instances.size());

    for (const auto& instance : scene.instances) {
        if (!instance) continue;
        uint8_t instanceType = fbs::NodeKind_NONE;
        auto offset = buildNode(fbb, *instance, instanceType);
        instanceTypes.push_back(instanceType);
        instances.push_back(offset);
    }

    auto ibl = fbs::CreateIblFBDirect(fbb, scene.ibl.path.c_str(), scene.ibl.intensity);

    auto root = fbs::CreateSceneFBDirect(
        fbb,
        scene.schemaVersion,
        scene.id,
        ibl,
        &materialTypes,
        &materials,
        &instanceTypes,
        &instances
    );

    // Carimba o file_identifier "LSCN" junto com a raiz.
    fbs::FinishSceneFBBuffer(fbb, root);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;

    file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
               static_cast<std::streamsize>(fbb.GetSize()));

    return file.good();
}

std::optional<SceneDTO> FlatBuffersSceneSerializer::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return std::nullopt;

    const std::streamsize size = file.tellg();
    if (size <= 0) return std::nullopt;
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return std::nullopt;

    // Verifica estrutura E identificador: conteúdo que não é nosso não passa.
    ::flatbuffers::Verifier verifier(buffer.data(), buffer.size());
    if (!fbs::VerifySceneFBBuffer(verifier)) return std::nullopt;

    const fbs::SceneFB* root = fbs::GetSceneFB(buffer.data());
    if (!root) return std::nullopt;

    if (root->schema_version() > SUPPORTED_SCHEMA_VERSION) return std::nullopt;

    SceneDTO scene;
    scene.schemaVersion = root->schema_version();
    scene.id            = root->id();

    if (const auto* ibl = root->ibl()) {
        if (ibl->path()) scene.ibl.path = ibl->path()->str();
        scene.ibl.intensity = ibl->intensity();
    }

    if (root->materials_type() && root->materials()) {
        const size_t count = std::min<size_t>(root->materials_type()->size(),
                                              root->materials()->size());
        for (size_t i = 0; i < count; ++i) {
            auto material = readMaterial(root->materials_type()->Get(static_cast<uint32_t>(i)),
                                         root->materials()->Get(static_cast<uint32_t>(i)));
            if (material) scene.materials.push_back(std::move(material));
        }
    }

    if (root->instances_type() && root->instances()) {
        const size_t count = std::min<size_t>(root->instances_type()->size(),
                                              root->instances()->size());
        for (size_t i = 0; i < count; ++i) {
            auto instance = readNode(root->instances_type()->Get(static_cast<uint32_t>(i)),
                                     root->instances()->Get(static_cast<uint32_t>(i)));
            if (instance) scene.instances.push_back(std::move(instance));
        }
    }

    return scene;
}

} // namespace lite
