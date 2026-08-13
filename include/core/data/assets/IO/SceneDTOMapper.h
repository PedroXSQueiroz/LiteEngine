#pragma once

#include <core/scene/Scene.h>
#include <core/data/assets/IO/Asset3dDTOMapper.h>
#include <core/data/DTOs/SceneDTO.h>

#include <typeinfo>
#include <typeindex>
#include <vector>
#include <memory>
#include <iterator>

namespace lite {

template<
    Asset3dConcept AssetType,
    TransformConcept TransformType,
    Asset3dInstanceFactoryConcept InstanceFactory,
    UIRendererConcept UIRenderer>

class SceneDTOMapper{

public:

    // Apagado por ponteiro da base (a main guarda um SceneDTOMapper<...>*).
    virtual ~SceneDTOMapper() = default;

    bool registerMapper(Asset3dDTOMapper* mapper)
    {
        for(Asset3dDTOMapper* current: m_mappers)
        {
            if(     ( current->getDTOTypeIndex() == mapper->getDTOTypeIndex() )
                ||  ( current->getEntityTypeIndex() == mapper->getEntityTypeIndex() ))
            {
                return false;
            }
        }

        m_mappers.push_back(mapper);

        return true;
    };

    SceneDTO toDto(Scene<
            AssetType,
            TransformType,
            InstanceFactory,
            UIRenderer
        >* scene)
    {
        SceneDTO sceneDto = buildBaseSceneDto(scene);

        std::vector<AssetType*> instances = scene->getAll();

        std::vector<std::unique_ptr<Asset3dInstanceDTO>> instancesDto = buildInstace3dDtos(instances);

        // Iteradores de MOVE: o intervalo é de unique_ptr, copiar é deletado.
        sceneDto.instances.insert(
            sceneDto.instances.end(),
            std::make_move_iterator(instancesDto.begin()),
            std::make_move_iterator(instancesDto.end())
        );

        return sceneDto;
    }

    std::vector<std::unique_ptr<Asset3dInstanceDTO>> buildInstace3dDtos(const std::vector<AssetType*>& instances)
    {
        std::vector<std::unique_ptr<Asset3dInstanceDTO>> dtos;

        for (AssetType* currentInstance : instances)
        {
            std::unique_ptr<Asset3dInstanceDTO> dto = instanceToDto(currentInstance);
            if(dto)
            {
                dtos.push_back(std::move(dto));
            }
        }

        return dtos;
    }

    // nullptr = nenhum mapper registrado para o tipo desta instância.
    std::unique_ptr<Asset3dInstanceDTO> instanceToDto(Node* currentInstance)
    {
        std::type_index assetTypeIndex = std::type_index(typeid(*currentInstance));

        Asset3dDTOMapper* mapperToAsset = getMapperByInstanceToDto(assetTypeIndex);

        if (mapperToAsset)
        {
            std::unique_ptr<Asset3dInstanceDTO> instance3dDto = mapperToAsset->nodeToDto(currentInstance);

            for(const std::unique_ptr<Node>& child : currentInstance->children)
            {
                std::unique_ptr<Asset3dInstanceDTO> childDto = instanceToDto(child.get());
                if(childDto)
                {
                    instance3dDto->children.push_back(std::move(childDto));
                }
            }

            return instance3dDto;
        }

        return nullptr;
    }

    virtual SceneDTO buildBaseSceneDto(Scene<
            AssetType,
            TransformType,
            InstanceFactory,
            UIRenderer
        >* scene) = 0;

private:

    // nullptr = nenhum mapper registrado para esse tipo de entidade.
    Asset3dDTOMapper* getMapperByInstanceToDto(std::type_index assetTypeIndex)
    {
        for(Asset3dDTOMapper* current: m_mappers)
        {
            if( current->getEntityTypeIndex() == assetTypeIndex )
            {
                return current;
            }
        }

        return nullptr;
    }

    std::vector<Asset3dDTOMapper*> m_mappers;

};

}
