#pragma once

#include <vector>
#include <set>
#include <map>

#include <core/concepts/UIRendererConcept.h>
#include <core/ui/elements/UIElements.h>

namespace lite {

    template<UIRendererConcept URI>
    class UIInstance {
    public:
        explicit UIInstance(URI* uiRenderer)
            : m_uiRenderer(uiRenderer) {}

        URI* getRenderer() { return m_uiRenderer; }

        virtual UIPanelElement<URI>* createRoot() = 0;

        int nextElementId() { return m_uiRenderer->nextElementId(); }

        UIPanelElement<URI>* start(){

            if(this->m_uiRenderer->start())
            {
                UIPanelElement<URI>* root = this->createRoot();
                if(root)
                {
                    //TODO: DRAW SHOULD REALLY BE CALLED HERE?
                    this->rootElementId = root->draw();
                }
                return root;
            }

            return nullptr;
        }

        UIElement<URI>* getElementById(int id){

            if(this->elements.contains(id))
            {
                return this->elements.at(id);
            }

            return nullptr;
        }

        void registerComponent(UIElement<URI>* element)
        {
            this->elements.insert(element->getId(), element);
        }

    protected:

        URI* m_uiRenderer;

        int rootElementId;

        std::map<int, UIElement<URI>*> elements;
    };

}
