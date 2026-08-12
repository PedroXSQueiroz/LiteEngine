#pragma once

#include <typeindex>
#include <memory>

#include <core/data/DTOs/Asset3dInstanceDTO.h>
#include <core/data/assets/Node.h>

namespace lite {

class Asset3dDTOMapper{

public:
    virtual std::type_index getEntityTypeIndex() = 0;

    virtual std::type_index getDTOTypeIndex() = 0;
    
    virtual Node* fromDto(Asset3dInstanceDTO dto) = 0;
    
    // std::unique_ptr<Asset3dInstanceDTO> toDto(Node* entity){
    //     std::unique_ptr<Asset3dInstanceDTO> currentNodeToDto = nodeToDto(entity);
        
    //     for(const std::unique_ptr<Node>& child : entity->children ) 
    //     {
    //         std::unique_ptr<Asset3dInstanceDTO> childDto = toDto(child.get());
    //         currentNodeToDto->children.push_back(childDto);
    //     }
        
    //     return currentNodeToDto;
    // }

private:
    virtual std::unique_ptr<Asset3dInstanceDTO> nodeToDto(Node* entity) = 0;

};

}