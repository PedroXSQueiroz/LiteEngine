#pragma once

// #include <fstream>
// #include <vector>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <utils/Entity.h>
#include <math/vec3.h>
#include <math/mat4.h>

#include <assimp/importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>

#include <lite/services/Asset3dLoader.h>
#include <lite/services/MaterialLoader.h>

using namespace std;
using namespace filament;
using namespace utils;

struct FBXMesh {
    utils::Entity entity;
    filament::Material* material = nullptr;
    filament::MaterialInstance* matInstance = nullptr;

    filament::VertexBuffer* vertexBff;
    filament::IndexBuffer* indexBff;

    filament::math::float3 center;
    float radius;
};


namespace lite {
    
    class Asset3dLoaderAssimp : public Asset3dLoader {
        
    public:

        Asset3dLoaderAssimp(
            filament::Engine* engine,
            filament::Scene* scene,
            MaterialLoader* matLoader

        ):Asset3dLoader(engine, scene),m_matLoader(matLoader){};

        virtual Asset3dData* load(std::string assetFilePath) override;

    private:

        void loadFBX(
            filament::Engine* engine, 
            const std::string& fbxPath, 
            filament::Scene* scene, 
            std::vector<FBXMesh>& objects, 
            const aiNode* currentSceneLevel = nullptr,
            const aiScene* aiScene = nullptr,
            const filament::math::mat4f& parentTransform = filament::math::mat4f(1.0f)
        );

        std::vector<Asset3dData*> processNode(
            const aiNode* node,
            const aiScene* scene,
            const filament::math::mat4f& parentTransform
        );

        filament::math::mat4f buildFilamentTransform(aiMatrix4x4 transform)
        {
            return filament::math::mat4f (
                transform.a1, transform.b1, transform.c1, transform.d1,
                transform.a2, transform.b2, transform.c2, transform.d2,
                transform.a3, transform.b3, transform.c3, transform.d3,
                transform.a4, transform.b4, transform.c4, transform.d4
            );
        };

        Assimp::Importer m_importer;
        MaterialLoader* m_matLoader;
    };

}