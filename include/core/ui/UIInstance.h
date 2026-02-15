#pragma once

#include <concepts>
#include <vector>
#include <set>
#include <type_traits>

#include <core/ui/UIRenderer.h>
#include <core/ui/elements/UIElements.h>

namespace lite {

    template<typename T>
    concept UIRendererForInstanceType = std::derived_from<T, UIRenderer<typename T::RendererType>>;

    // Conceito: E deve derivar de UIElement<URT>
    template<typename E, typename URT>
    concept UIElementType = std::derived_from<E, UIElement<URT>>;

    template<UIRendererForInstanceType URI>
    class UIInstance {
    public:
        explicit UIInstance(URI* uiRenderer)
            : m_uiRenderer(uiRenderer) {}

        URI* getRenderer() { return m_uiRenderer; }

        virtual UIPanelElement<URI>* createRoot() = 0; 

        UIPanelElement<URI>* start(){
            
            if(this->m_uiRenderer->start())
            {
                UIPanelElement<URI>* root = this->instantiateElement<UIPanelElement<URI>>(this->createRoot());
                if(root)
                {
                    this->rootElementId = root->getId();
                }
                return root;
            }

            return nullptr;
        }

        // template<EditorElementType<R> E>
        UIElement<URI>* getElementById(int id){

            if(this->elementsIds.contains(id))
            {
                return this->getElementByIdFromRenderer(id);
            }

            return nullptr;
        }

        template<typename ET>
            requires UIElementType<ET, URI>
        ET* instantiateElement(ET* element){
            
            if(element && !this->elementsIds.contains(element->getId()))
            {
                int id = element->draw();
                
                this->elementsIds.insert( id );
                
                return element;
            }
            
            return nullptr;
        }

    protected:
        
        URI* m_uiRenderer;

        virtual UIElement<URI>* getElementByIdFromRenderer(int id) = 0;

        int rootElementId;

        std::set<int> elementsIds;
    };

}
