#pragma once

#include <typeindex>
#include <memory>

#include <core/data/DTOs/Asset3dInstanceDTO.h>
#include <core/data/assets/Node.h>

namespace lite {

class Asset3dDTOMapper{

public:
    // Apagado por ponteiro da base (o registry guarda Asset3dDTOMapper*).
    virtual ~Asset3dDTOMapper() = default;

    virtual std::type_index getEntityTypeIndex() = 0;

    virtual std::type_index getDTOTypeIndex() = 0;

    // Por referência const: Asset3dInstanceDTO não é copiável nem movível
    // (tem vector<unique_ptr> children e destrutor declarado), então nenhuma
    // chamada compilaria com o parâmetro por valor.
    virtual Node* fromDto(const Asset3dInstanceDTO& dto) = 0;

    // unique_ptr, e não valor, porque Asset3dData é polimórfica: devolver por
    // valor FATIARIA a filha (um MeshAsset3dData perderia geometria, bounds e
    // materialName no return) — e nem compilaria, já que a classe não é
    // copiável (children é vector<unique_ptr>) nem movível (destrutor
    // declarado). Mesmo idioma de Asset3dData::clone().
    virtual std::unique_ptr<Asset3dData> fromDtoToData(const Asset3dInstanceDTO& dto) = 0;
    
    // std::unique_ptr<Asset3dInstanceDTO> toDto(Node* entity){
    //     std::unique_ptr<Asset3dInstanceDTO> currentNodeToDto = nodeToDto(entity);
        
    //     for(const std::unique_ptr<Node>& child : entity->children ) 
    //     {
    //         std::unique_ptr<Asset3dInstanceDTO> childDto = toDto(child.get());
    //         currentNodeToDto->children.push_back(childDto);
    //     }
        
    //     return currentNodeToDto;
    // }
    virtual std::unique_ptr<Asset3dInstanceDTO> nodeToDto(Node* entity) = 0;

private:

};

}