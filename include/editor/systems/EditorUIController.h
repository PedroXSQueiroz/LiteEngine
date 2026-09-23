#pragma once

#include <core/SceneScopeSystem.h>
#include <core/concepts/EngineConcepts.h>
#include <core/data/assets/Node.h>
#include <core/data/assets/Asset3dInstance.h>
#include <core/ui/UIInstance.h>
#include <core/ui/elements/UIElements.h>

#include <memory>
#include <string>

namespace lite{

    // Controller da UI do editor. Hoje só popula a treeview de objetos da cena,
    // mas a ideia é concentrar a UI do editor em geral — por isso recebe o
    // UIInstance, e não um elemento específico.
    //
    // REGRA DE DESIGN (docs/ARCHITECTURE.md §8): a UI NÃO observa a cena. A
    // carga inicial acontece uma única vez, no postInit. Com a cena "viva" em
    // runtime, toda adição/remoção/atualização na cena que deve aparecer na UI
    // é explícita: primeiro na cena, depois na UI através deste controller
    // (a API dessas mudanças ainda será desenhada).
    //
    // SceneType:      a cena concreta (ex.: FilamentScene), recebida pelo
    //                 construtor.
    // TransformType:  o transform concreto dos nós; o label de cada nó vem do
    //                 name de Asset3dInstance<TransformType>, a base comum de
    //                 raízes e meshes.
    // UIRendererType: o renderer de UI concreto (ex.: CEF_Filament_UIRendererThreaded).
    //
    // THREADING: postInit roda na render thread (dentro de Scene::update). A
    // tree não tem lock — o createNode escreve nos nós dela e dispara o
    // executeJavaScript a partir dessa thread.
    template<SceneConcept SceneType, TransformConcept TransformType, UIRendererConcept UIRendererType>
    class EditorUIController: public SceneScopeSystem {

    public:

        // sceneTreeId: id do elemento da tree no UIInstance. Só é definitivo
        // depois do draw() (todo draw gera id novo), e a tree precisa ter sido
        // registrada com UIInstance::registerComponent.
        EditorUIController(SceneType* scene, UIInstance<UIRendererType>* uiInstance, int sceneTreeId)
            :   m_scene(scene)
            ,   m_uiInstance(uiInstance)
            ,   m_sceneTreeId(sceneTreeId) {};

        virtual bool postInit() override {

            if(!this->m_scene) return false;

            UITreeElement<UIRendererType, Node*>* sceneTree =
                dynamic_cast<UITreeElement<UIRendererType, Node*>*>(this->m_uiInstance->getElementById(this->m_sceneTreeId));
            if(!sceneTree) return false;

            for(Node* currentInstance : this->m_scene->getAll())
            {
                populateSceneTree(sceneTree, currentInstance);
            }

            return true;
        }

    private:

        void populateSceneTree(UITreeElement<UIRendererType, Node*>* treeScene, Node* currentInstance, int parentNodeId = -1)
        {
            Asset3dInstance<TransformType>* asset = dynamic_cast<Asset3dInstance<TransformType>*>(currentInstance);
            std::string label = asset ? asset->name : "";

            auto currentNode = treeScene->createNode(label, currentInstance, parentNodeId);

            if(currentNode.has_value())
            {
                for(std::unique_ptr<Node>& child : currentInstance->children)
                {
                    populateSceneTree(treeScene, child.get(), currentNode.value().id);
                }
            }
        }

        SceneType* m_scene;
        UIInstance<UIRendererType>* m_uiInstance;
        int m_sceneTreeId;
    };

}
