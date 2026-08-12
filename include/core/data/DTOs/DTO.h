#pragma once

#include <typeindex>
#include <vector>

using namespace std;

namespace lite {

struct DTO {
    
public:

    std::type_index getEntityTypeId()
    {
        return m_entityTypeId;
    };
    
    std::vector<DTO> children;

protected:

    std::type_index m_entityTypeId;

};

}