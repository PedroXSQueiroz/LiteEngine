#pragma once

#include <core/concepts/EngineConcepts.h>

#include <core/scene/Scene.h>
#include <core/utils/TransformUtils.h>
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
    SceneConcept SceneType>

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

    // scene por ponteiro NÃO-const: create() não é const.
    // dto por referência const: SceneDTO tem vector<unique_ptr>, copiar é deletado.
    void populateFromDto(SceneType* scene, const SceneDTO& dto)
    {
        populateInstancesIntoSceneFromDto(scene, dto.instances, nullptr);
    }

    // instancesDtos por referência const pelo mesmo motivo — e porque este
    // método só LÊ os DTOs, a posse continua sendo de quem chamou.
    // root nulo = nível raiz (não há pai a quem pendurar).
    void populateInstancesIntoSceneFromDto(
        SceneType* scene,
        const std::vector<std::unique_ptr<lite::Asset3dInstanceDTO>>& instancesDtos,
        Node* root)
    {
        for( const std::unique_ptr<lite::Asset3dInstanceDTO>& currentInstanceDto: instancesDtos )
        {
            // typeid(*dto) dá o tipo DINÂMICO do DTO. O readNode do serializer
            // já constrói a subclasse concreta certa a partir da tag da union,
            // então a informação está aqui sem depender de campo gravado — o
            // assetTypeIndex não atravessa o arquivo (type_index embrulha um
            // ponteiro válido só nesta execução).
            Asset3dDTOMapper* mapperToAsset =
                getMapperByDtoToData(std::type_index(typeid(*currentInstanceDto)));

            std::unique_ptr<Asset3dData> instanceData = mapperToAsset->fromDtoToData(*currentInstanceDto);

            // Transform NOVO, não o do pai: o factory religa este wrapper ao
            // entity recém-criado (rootTransform.of(...)), e a pose do nó já
            // viaja no instanceData — é o localTransform que veio do DTO.
            int newAssetId = scene->create(
                *instanceData,
                std::vector<std::unique_ptr<MaterialData>>(),
                TransformUtils<TransformType>::build()
            );

            // get() devolve a INSTÂNCIA (AssetType), não o dado. Bloqueia até a
            // render thread instanciar o id.
            AssetType* newAsset = scene->get(newAssetId);

            if(root)
            {
                root->addChild(newAsset);
            }

            populateInstancesIntoSceneFromDto(
                scene,
                currentInstanceDto->children,
                newAsset
            );

        }
    }

    SceneDTO toDto(SceneType* scene)
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
            instance3dDto->assetTypeIndex = assetTypeIndex;

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

    virtual SceneDTO buildBaseSceneDto(SceneType* scene) = 0;

private:

    // Direção de LEITURA: casa pelo tipo do DTO, não pelo da entidade.
    // nullptr = nenhum mapper registrado para esse tipo de DTO.
    Asset3dDTOMapper* getMapperByDtoToData(std::type_index dtoTypeIndex)
    {
        for(Asset3dDTOMapper* current: m_mappers)
        {
            if( current->getDTOTypeIndex() == dtoTypeIndex )
            {
                return current;
            }
        }

        return nullptr;
    }

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
