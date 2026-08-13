#include <core/data/assets/IO/DummySceneSerializer.h>

#include <core/data/DTOs/CameraAsset3dInstanceDTO.h>
#include <core/data/DTOs/MeshAsset3dInstanceDTO.h>

#include <iostream>

namespace lite {

namespace {

std::string indent(int depth) {
    return std::string(static_cast<size_t>(depth) * 2, ' ');
}

void printVec3(const char* label, const glm::vec3& v) {
    std::cout << label << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

void printVec4(const char* label, const glm::vec4& v) {
    std::cout << label << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
}

// glm é column-major (m[coluna][linha]); imprime por LINHA para leitura humana.
void printMatrix(const glm::mat4& m, int depth) {
    for (int row = 0; row < 4; ++row) {
        std::cout << indent(depth) << "| "
                  << m[0][row] << " " << m[1][row] << " "
                  << m[2][row] << " " << m[3][row] << " |" << std::endl;
    }
}

void printTexture(const char* label, const std::optional<TextureInfoDTO>& tex, int depth) {
    if (!tex.has_value()) return;
    std::cout << indent(depth) << label << ": " << tex->path
              << " (sRGB=" << (tex->sRGB ? "true" : "false")
              << ", " << tex->width << "x" << tex->height
              << ", " << tex->channels << " ch)" << std::endl;
}

void printMaterial(const MaterialDTO& material, int depth) {
    std::cout << indent(depth) << "- name: " << material.name << std::endl;

    const auto* pbr = dynamic_cast<const MPBRLitMaterialDTO*>(&material);
    if (!pbr) {
        std::cout << indent(depth + 1) << "(tipo sem impressão específica)" << std::endl;
        return;
    }

    std::cout << indent(depth + 1) << "tipo: MPBRLit" << std::endl;
    std::cout << indent(depth + 1);
    printVec4("baseColorFactor: ", pbr->baseColorFactor);
    std::cout << std::endl;
    std::cout << indent(depth + 1) << "metallicFactor: " << pbr->metallicFactor << std::endl;
    std::cout << indent(depth + 1) << "roughnessFactor: " << pbr->roughnessFactor << std::endl;
    std::cout << indent(depth + 1);
    printVec3("emissiveFactor: ", pbr->emissiveFactor);
    std::cout << std::endl;

    printTexture("baseColorTexture", pbr->baseColorTexture, depth + 1);
    printTexture("normalTexture", pbr->normalTexture, depth + 1);
    printTexture("metallicRoughnessTexture", pbr->metallicRoughnessTexture, depth + 1);
    printTexture("occlusionTexture", pbr->occlusionTexture, depth + 1);
    printTexture("emissiveTexture", pbr->emissiveTexture, depth + 1);
}

void printInstance(const Asset3dInstanceDTO& instance, int depth) {
    std::cout << indent(depth) << "- [" << instance.id << "] "
              << (instance.name.empty() ? "(sem nome)" : instance.name)
              << (instance.visible ? "" : " (invisível)") << std::endl;

    std::cout << indent(depth + 1) << "localTransform:" << std::endl;
    printMatrix(instance.localTransform, depth + 2);

    // Campos de cada tipo concreto. A geometria sai como CONTAGEM: despejar
    // milhares de vértices tornaria a saída inútil para conferência.
    if (const auto* mesh = dynamic_cast<const MeshAsset3dInstanceDTO*>(&instance)) {
        std::cout << indent(depth + 1) << "tipo: Mesh" << std::endl;
        std::cout << indent(depth + 1) << "positions: " << mesh->positions.size()
                  << " | normals: " << mesh->normals.size()
                  << " | uvs: " << mesh->uvs.size()
                  << " | indices: " << mesh->indices.size() << std::endl;
        std::cout << indent(depth + 1);
        printVec3("boundsMin: ", mesh->boundsMin);
        std::cout << "  ";
        printVec3("boundsMax: ", mesh->boundsMax);
        std::cout << std::endl;
        std::cout << indent(depth + 1) << "materialName: " << mesh->materialName << std::endl;
    } else if (const auto* camera = dynamic_cast<const CameraAsset3dInstanceDTO*>(&instance)) {
        std::cout << indent(depth + 1) << "tipo: Camera" << std::endl;
        std::cout << indent(depth + 1);
        printVec3("eye: ", camera->eye);
        std::cout << "  ";
        printVec3("target: ", camera->target);
        std::cout << std::endl;
    } else {
        std::cout << indent(depth + 1) << "tipo: Asset3dInstance (nó sem dados extras)" << std::endl;
    }

    std::cout << indent(depth + 1) << "children: " << instance.children.size() << std::endl;
    for (const auto& child : instance.children) {
        if (!child) {
            std::cout << indent(depth + 2) << "- (nullptr)" << std::endl;
            continue;
        }
        printInstance(*child, depth + 2);
    }
}

} // namespace

bool DummySceneSerializer::save(const SceneDTO& scene, const std::string& path) {
    std::cout << "===== DummySceneSerializer::save =====" << std::endl;
    std::cout << "path (ignorado, nada é gravado): " << path << std::endl;
    std::cout << "schemaVersion: " << scene.schemaVersion << std::endl;
    std::cout << "id: " << scene.id << std::endl;
    std::cout << "ibl.path: " << scene.ibl.path << std::endl;
    std::cout << "ibl.intensity: " << scene.ibl.intensity << std::endl;

    std::cout << "materials: " << scene.materials.size() << std::endl;
    for (const auto& material : scene.materials) {
        if (!material) {
            std::cout << indent(1) << "- (nullptr)" << std::endl;
            continue;
        }
        printMaterial(*material, 1);
    }

    std::cout << "instances: " << scene.instances.size() << std::endl;
    for (const auto& instance : scene.instances) {
        if (!instance) {
            std::cout << indent(1) << "- (nullptr)" << std::endl;
            continue;
        }
        printInstance(*instance, 1);
    }

    std::cout << "===== fim =====" << std::endl;

    return true;
}

std::optional<SceneDTO> DummySceneSerializer::load(const std::string& path) {
    std::cout << "DummySceneSerializer::load: nada a ler (" << path << ")" << std::endl;
    return std::nullopt;
}

} // namespace lite
