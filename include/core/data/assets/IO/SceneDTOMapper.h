#pragma once

#include <core/scene/Scene.h>
#include <core/data/assets/IO/Asset3dDTOMapper.h>
#include <core/data/DTOs/SceneDTO.h>

#include <typeinfo>
#include <typeindex>
#include <vector>

namespace lite {

template<
    Asset3dConcept AssetType,
    TransformConcept TransformType,
    Asset3dInstanceFactoryConcept InstanceFactory,
    UIRendererConcept UIRenderer>

class SceneDTOMapper{

public:

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

        sceneDto.instances.insert(sceneDto.instances.end(), instancesDto.begin(), instancesDto.end());
    }

    std::vector<std::unique_ptr<Asset3dInstanceDTO>> buildInstace3dDtos(std::vector<AssetType*> instances)
    {
        std::vector<std::unique_ptr<Asset3dInstanceDTO>> dtos;

        for (AssetType* currentInstance : instances)
        {


            // std::type_index assetTypeIndex = std::type_index(typeid(*currentInstance.get()));

            // std::optional<Asset3dDTOMapper*> mapperToAsset = getMapperByInstanceToDto(assetTypeIndex);

            // if (mapperToAsset.has_value())
            // {
            //     Node* nodeOfInstance = dynamic_cast<Node*>(currentInstance.get());
            //     std::unique_ptr<Asset3dInstanceDTO> instance3dDto = mapperToAsset.value()->nodeToDto(nodeOfInstance);
            //     dtos.push_back(instance3dDto);

            //     for(std::unique_ptr<Node> child : currentInstance->children)
            //     {
            //         AssetType* childNode = dynamic_cast<AssetType*>(child.get());
            //         buildInstace3dDtos(std::vector<std::unique_ptr<AssetType>>{childNode});
            //     }

            // }

            std::optional<std::unique_ptr<Asset3dInstanceDTO>> dto = instanceToDto(currentInstance);
            if(dto.has_value())
            {
                dtos.push_back(std::move(dto.value()));
            }
        }

        return dtos;
    }

    std::optional<std::unique_ptr<Asset3dInstanceDTO>> instanceToDto(Node* currentInstance)
    {
        std::type_index assetTypeIndex = std::type_index(typeid(*currentInstance));

        std::optional<Asset3dDTOMapper*> mapperToAsset = getMapperByInstanceToDto(assetTypeIndex);

        if (mapperToAsset.has_value())
        {
            Node* nodeOfInstance = dynamic_cast<Node*>(currentInstance);
            std::unique_ptr<Asset3dInstanceDTO> instance3dDto = mapperToAsset.value()->nodeToDto(nodeOfInstance);
            // dtos.push_back(instance3dDto);
            
            for(const std::unique_ptr<Node>& child : currentInstance->children)
            {
                // AssetType* childNode = dynamic_cast<AssetType*>(child.get());
                std::optional<std::unique_ptr<Asset3dInstanceDTO>> childDto = instanceToDto(child.get());
                if(childDto.has_value())
                {
                    instance3dDto.get()->children.push_back(std::move(childDto.value()));
                }
            }

            return std::optional<std::unique_ptr<Asset3dInstanceDTO>>( std::move(instance3dDto) );
        }

        return std::nullopt;
    }

    virtual SceneDTO buildBaseSceneDto(Scene<
            AssetType,
            TransformType,
            InstanceFactory,
            UIRenderer
        >* scene) = 0;

private:

    std::optional<Asset3dDTOMapper*> getMapperByInstanceToDto(std::type_index assetTypeIndex)
    {
        for(Asset3dDTOMapper* current: m_mappers)
        {
            if( current->getEntityTypeIndex() == assetTypeIndex )
            {
                return std::optional<Asset3dDTOMapper*>(current);
            }
        }

        return std::optional<Asset3dDTOMapper*>();
    }

    std::vector<Asset3dDTOMapper*> m_mappers;

};

}