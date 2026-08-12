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
            if(     ( current->getDTOTypeIndex() == mapper.getDTOTypeIndex() ) 
                ||  ( current->getEntityTypeIndex() == mapper.getEntityTypeIndex() ))
            {
                return false;
            }
        }
        
        m_mappers.push_back(mapper);

        return true;
    };

    SceneDTO toDto(std::unique_ptr<Scene<
            AssetType,
            TransformType,
            InstanceFactory,
            UIRenderer
        >> scene)
    {
        SceneDTO sceneDto = buildBaseSceneDto(scene);
        
        std::vector<std::unique_ptr<AssetType>> instances = scene.get()->getAll();

        std::vector<std::unique_ptr<Asset3dInstanceDTO>> instancesDto = buildInstace3dDtos(instances);

        sceneDto.instances.insert(sceneDto.instances.end(), instancesDto.begin(), instancesDto.end());
    }

    std::vector<std::unique_ptr<Asset3dInstanceDTO>> buildInstace3dDtos(std::unique_ptr<Node> &instances)
    {
        std::vector<std::unique_ptr<Asset3dInstanceDTO>> dtos;
        
        for (std::unique_ptr<Node> currentInstance : instances)
        {
            std::type_index assetTypeIndex = std::type_index(typeid(*currentInstance.get()));

            std::optional<Asset3dDTOMapper> mapperToAsset = getMapperByInstanceToDto(assetTypeIndex);

            if (mapperToAsset.has_value())
            {
                std::unique_ptr<Asset3dInstanceDTO> instance3dDto = mapperToAsset.value().nodeToDto(currentInstance);
                dtos.push_back(instance3dDto);
                
                instance3dDto->children = buildInstace3dDtos(currentInstance->children);
            }
        }

        return dtos;
    }

    virtual SceneDTO buildBaseSceneDto(std::unique_ptr<Scene<
            AssetType,
            TransformType,
            InstanceFactory,
            UIRenderer
        >> scene) = 0;
    
private:

    std::optional<Asset3dDTOMapper*> getMapperByInstanceToDto(std::type_index assetTypeIndex)
    {
        for(Asset3dDTOMapper* current: m_mappers)
        {
            if( current.getEntityTypeIndex() == assetTypeIndex ) 
            {
                return std::optional<Asset3dDTOMapper*>(current);
            }
        }

        return std::optional<Asset3dDTOMapper*>();
    }

    std::vector<Asset3dDTOMapper*> m_mappers;

};

}