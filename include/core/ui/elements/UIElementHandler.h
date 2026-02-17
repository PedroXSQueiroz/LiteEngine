#pragma once

#include <typeindex>
#include <stdexcept>

namespace lite {

    class UIElementHandler {
        
    public:
        template<typename E>
        UIElementHandler(E* elementPtr)
            : m_elementPtr(static_cast<void*>(elementPtr))
            , m_typeIndex(typeid(E)) {}

            
        template<typename T>
        void invokeEvents(std::string eventName);
            
    private:
        
        void* m_elementPtr;
        std::type_index m_typeIndex;
    };

}
