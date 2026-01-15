#pragma once

#include <Asset3dLoaderAssimp.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/RenderableManager.h>
#include <utils/EntityManager.h>

#include <iostream>
    
using namespace lite;
using namespace filament;
using namespace std;


Asset3dData* Asset3dLoaderAssimp::load(std::string fileAssetPath){
    
    filament::Engine*   currentEngine = this->getEngine();
    filament::Scene*    currentScene = this->getScene();

    std::vector<FBXMesh> objects;
    
    this->loadFBX(currentEngine, fileAssetPath, currentScene, objects);

    return new Asset3dData(objects);
}

void Asset3dLoaderAssimp::loadFBX(
    filament::Engine* engine, 
    const std::string& fbxPath, 
    filament::Scene* scene, 
    std::vector<FBXMesh>& objects, 
    const aiNode* currentSceneLevel,
    const aiScene* aiScene,
    const filament::math::mat4f& parentTransform
){

    FBXMesh result;
    result.entity = utils::Entity(); // Inicializar como nulo

    Assimp::Importer importer;

    if(!aiScene)
    {
        aiScene = importer.ReadFile(fbxPath,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace);

    }

    if (!aiScene || !aiScene->HasMeshes()) {
        std::cerr << "Erro ao carregar FBX: " << importer.GetErrorString() << std::endl;
        return;
    }

    if(!currentSceneLevel)
    {
        currentSceneLevel = aiScene->mRootNode;
    }

    filament::math::mat4f currentRelativeTransform = buildFilamentTransform(currentSceneLevel->mTransformation);

    filament::math::mat4f  currentAbsoluteTransform = parentTransform * currentRelativeTransform;

    for( int currentMeshIndex = 0; currentMeshIndex < currentSceneLevel->mNumMeshes; currentMeshIndex++) 
    {
        std::cout << "FBX loaded, meshes: " << aiScene->mNumMeshes << std::endl;

        int currentSceneMeshIndex = currentSceneLevel->mMeshes[currentMeshIndex];
        aiMesh* mesh = aiScene->mMeshes[currentSceneMeshIndex];
        std::cout << "Vertices: " << mesh->mNumVertices << ", Faces: " << mesh->mNumFaces << std::endl;

        // ---------------------------
        // Carregar material default
        // ---------------------------
        std::string matPath = "D:/Workspace/LiteEngine/3rd_party/filament/out/filament/generated/material/defaultMaterial.filamat";
        std::ifstream matFile(matPath, std::ios::binary);
        
        if (!matFile.is_open()) {
            std::cerr << "Failed to open material file: " << matPath << std::endl;
            return;
        }
        
        const aiMaterial* aiMat = aiScene->mMaterials[mesh->mMaterialIndex];
        size_t lastSlash = fbxPath.find_last_of("/\\");
        std::string baseDirectory = (lastSlash != std::string::npos) ? fbxPath.substr(0, lastSlash) : ".";

        MaterialData* matData = this->m_matLoader->loadFromAssimp(aiMat, baseDirectory);
        
        // std::vector<uint8_t> matData((std::istreambuf_iterator<char>(matFile)), {});
        // matFile.close();
        
        // std::cout << "Material loaded, size: " << matData.size() << " bytes" << std::endl;
        // filament::Material* mat = filament::Material::Builder()
        //     .package(matData.data(), matData.size())
        //     .build(*engine);

        // if (!mat) {
        //     std::cerr << "Failed to create material!" << std::endl;
        //     return;
        // }

        if(!matData){
            std::cerr << "Failed to create material!" << std::endl;
            return;
        }

        filament::MaterialInstance* matInstance = matData->material->createInstance();
        std::cout << "Material instance created" << std::endl;

        // ---------------------------
        // Preparar dados (com cópias permanentes usando new)
        // ---------------------------
        auto* positions = new std::vector<filament::math::float3>(mesh->mNumVertices);
        auto* normals = new std::vector<filament::math::float3>(mesh->mNumVertices);
        auto* uvs = new std::vector<filament::math::float2>(mesh->mNumVertices);

        filament::math::float3 boundingBoxMin(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        );
        filament::math::float3 boundingBoxMax(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );

        for (size_t i = 0; i < mesh->mNumVertices; ++i) {
            (*positions)[i] = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};

            boundingBoxMin.x = std::min(boundingBoxMin.x, mesh->mVertices[i].x);
            boundingBoxMin.y = std::min(boundingBoxMin.y, mesh->mVertices[i].y);
            boundingBoxMin.z = std::min(boundingBoxMin.z, mesh->mVertices[i].z);
            boundingBoxMax.x = std::max(boundingBoxMax.x, mesh->mVertices[i].x);
            boundingBoxMax.y = std::max(boundingBoxMax.y, mesh->mVertices[i].y);
            boundingBoxMax.z = std::max(boundingBoxMax.z, mesh->mVertices[i].z);

            if (mesh->HasNormals()) {
                (*normals)[i] = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
            } else {
                (*normals)[i] = {0.f, 1.f, 0.f};
            }

            if (mesh->HasTextureCoords(0)) {
                (*uvs)[i] = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
            } else {
                (*uvs)[i] = {0.f, 0.f};
            }
        }

        result.center = (boundingBoxMin + boundingBoxMax) * 0.5f;
        result.radius = length(boundingBoxMax - boundingBoxMin) * 0.5f;

        std::cout << "Bounding box: min(" << boundingBoxMin.x << ", " << boundingBoxMin.y << ", " << boundingBoxMin.z << ")" << std::endl;
        std::cout << "             max(" << boundingBoxMax.x << ", " << boundingBoxMax.y << ", " << boundingBoxMax.z << ")" << std::endl;
        std::cout << "Center: " << result.center.x << ", " << result.center.y << ", " << result.center.z << std::endl;
        std::cout << "Radius: " << result.radius << std::endl;

        // ---------------------------
        // Criar VertexBuffer
        // ---------------------------
        filament::VertexBuffer* vertexBuffer = filament::VertexBuffer::Builder()
            .vertexCount(mesh->mNumVertices)
            .bufferCount(3)
            .attribute(filament::VertexAttribute::POSITION, 0, 
                    filament::VertexBuffer::AttributeType::FLOAT3, 0, 12)
            .attribute(filament::VertexAttribute::TANGENTS, 1, 
                    filament::VertexBuffer::AttributeType::FLOAT3, 0, 12)  // Simplificado - normais como tangentes
            .attribute(filament::VertexAttribute::UV0, 2, 
                    filament::VertexBuffer::AttributeType::FLOAT2, 0, 8)
            .build(*engine);

        // Usar callbacks para liberar memória quando Filament terminar
        vertexBuffer->setBufferAt(*engine, 0,
            filament::VertexBuffer::BufferDescriptor(
                positions->data(),
                positions->size() * sizeof(filament::math::float3),
                [](void* buffer, size_t size, void* user) {
                    delete static_cast<std::vector<filament::math::float3>*>(user);
                },
                positions
            )
        );

        vertexBuffer->setBufferAt(*engine, 1,
            filament::VertexBuffer::BufferDescriptor(
                normals->data(),
                normals->size() * sizeof(filament::math::float3),
                [](void* buffer, size_t size, void* user) {
                    delete static_cast<std::vector<filament::math::float3>*>(user);
                },
                normals
            )
        );

        vertexBuffer->setBufferAt(*engine, 2,
            filament::VertexBuffer::BufferDescriptor(
                uvs->data(),
                uvs->size() * sizeof(filament::math::float2),
                [](void* buffer, size_t size, void* user) {
                    delete static_cast<std::vector<filament::math::float2>*>(user);
                },
                uvs
            )
        );

        std::cout << "Vertex buffer created" << std::endl;

        // ---------------------------
        // Criar IndexBuffer
        // ---------------------------
        auto* indices = new std::vector<uint32_t>();
        for (size_t f = 0; f < mesh->mNumFaces; f++) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices == 3) {
                indices->push_back(face.mIndices[0]);
                indices->push_back(face.mIndices[1]);
                indices->push_back(face.mIndices[2]);
            }
        }

        std::cout << "Index count: " << indices->size() << " (" << indices->size() / 3 << " triangles)" << std::endl;

        if (indices->empty()) {
            std::cerr << "No valid triangles found!" << std::endl;
            delete indices;
            engine->destroy(vertexBuffer);
            return;
        }

        filament::IndexBuffer* indexBuffer = filament::IndexBuffer::Builder()
            .indexCount(indices->size())
            .bufferType(filament::IndexBuffer::IndexType::UINT)
            .build(*engine);

        indexBuffer->setBuffer(*engine,
            filament::IndexBuffer::BufferDescriptor(
                indices->data(),
                indices->size() * sizeof(uint32_t),
                [](void* buffer, size_t size, void* user) {
                    delete static_cast<std::vector<uint32_t>*>(user);
                },
                indices
            )
        );

        std::cout << "Index buffer created" << std::endl;

        // ---------------------------
        // Criar Entity e Renderable
        // ---------------------------
        utils::Entity entity = utils::EntityManager::get().create();

        filament::RenderableManager::Builder(1)
            .boundingBox({{boundingBoxMin}, {boundingBoxMax}})
            .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
                    vertexBuffer, indexBuffer, 0, indices->size())
            .material(0, matInstance)  // IMPORTANTE: aplicar material
            .culling(false)
            .castShadows(false)
            .receiveShadows(true)
            .build(*engine, entity);

        scene->addEntity(entity);

        result.entity = entity;
        result.material = matData->material;
        result.matInstance = matInstance;
        result.vertexBff = vertexBuffer;
        result.indexBff = indexBuffer;

        std::cout << "FBX mesh created successfully!" << std::endl;
        std::cout << "Entity ID: " << entity.getId() << std::endl;
        
        auto& currentTransMan = engine->getTransformManager();
        currentTransMan.create(entity);
        filament::TransformManager::Instance trannsformInst = currentTransMan.getInstance(entity);

        if(trannsformInst)
        {
            currentTransMan.setTransform(trannsformInst, currentAbsoluteTransform);
        }


        objects.push_back( result );
    }

    //Drill Down scene nodes
    for( int childrenIndex = 0; childrenIndex < currentSceneLevel->mNumChildren; childrenIndex++) 
    {
        aiNode* subLevel = currentSceneLevel->mChildren[childrenIndex];
        filament::math::mat4f childTransform = buildFilamentTransform(subLevel->mTransformation);
        
        // subLevel->mTransformation
        
        this->loadFBX(
            engine,
            fbxPath,
            scene,
            objects,
            subLevel,
            aiScene,
            childTransform
        );
        
    }

}