#pragma once

#include <memory>
#include <map>
#include <vector>
#include <functional>

#include <core/concepts/EngineConcepts.h>
#include <core/assets/instanceFactory/Asset3dInstanceFactory.h>
#include <core/data/assets/Asset3dData.h>
#include <core/SceneScopeSystem.h>

using namespace std;


namespace lite
{
    template<
        Asset3dConcept AssetType, 
        TransformConcept TransformType, 
        Asset3dInstanceFactoryConcept InstanceFactory,
        UIRendererConcept UIRenderer>
    class Scene {
        
        public:
        using Asset3DReference = std::unique_ptr<AssetType>;

        Scene(
            std::unique_ptr<InstanceFactory> asset3dFactory,
            std::unique_ptr<UIRenderer> uiRenderer
        )
        : m_asset3dFactory(std::move(asset3dFactory))
        , m_uiRenderer(std::move(uiRenderer)){}

        int create(const Asset3dData& data, const std::vector<MaterialData>& materials, TransformType transform, AssetType*& instance) {
            Asset3DReference newInstance = this->m_asset3dFactory->instantiateAsset(data, transform, materials);
            instance = newInstance.get();

            m_3dInstances.emplace(++m_lastId, std::move(newInstance));
            return m_lastId;
        }

        AssetType* get(int id) {
            auto it = m_3dInstances.find(id);
            return it != m_3dInstances.end() ? it->second.get() : nullptr;
        }

        std::vector<AssetType*> find(std::function<bool(AssetType*)> criteria) {
            std::vector<AssetType*> result;
            for (const auto& [key, value] : m_3dInstances) {
                if (criteria(value.get())) result.push_back(value.get());
            }
            return result;
        }

        bool destroy(int id) {
            
            if(this->m_3dInstances.contains(id))
            {
                if( this->m_asset3dFactory->destroyAsset(this->m_3dInstances.at(id).get()) ) 
                {
                    // this->m_3dInstances.erase(id);
                    return true;
                }
            }

            return false;
        }

        bool destroy(Asset3DReference instance) {
            
            //FIXME: TEM QUE HAVER UMA FORMA MAIS EFICIENTE DE FAZER ISSO, 
            // A CENA PODE TER MUITOS OBETOS E SERÁ RUIM ITERAR EM TODOS
            for (auto it = m_3dInstances.begin(); it != m_3dInstances.end(); ++it) {
                if (it->second.get() == instance.get()) {
                    if( this->m_asset3dFactory->destroyAsset(it->second.get()) )
                    {
                        // this->m_3dInstances.erase(it);
                        return true;
                    }
                }
            }
            
            return false;
        }

        virtual bool prepareRender() { return true; }
        virtual bool finishRender() { return true; } 

        bool update(float deltaTime = 0.0f)
        {
            /*TODO: MAIN LOOP VAI SER AQUI?
                    CASO SIM, TALVEZ O CICLO SEJA
                    1. PROCESSA INPUT
                        1.1 VAIU SER NECESSÁRIO UMA INTERFACE COMUM PARA INPUT ?
                    2. PROCESSA LÓGICA DE GAME (NO MOMENTO, SÓ A ATUALIZAÇÃO DO CURSOR)
                        2.1 COMO FAZER ISSO AGNÓSTICO?
                        2.2 NO FUTURO ELE VAI LUPAR ENTRE OS ASSETS E INVOCAR AS LÓGICAS DELES E DE SEUS COMPONENTES
                    3. RENDERIZA UI
                    4. LIMPA OBJETOS DELETADOS
                    5. RENDERIZA CENA
            */

            for(std::function<void()> callback : m_preRenderCallback) callback();

            if(this->prepareRender())
            {
                //FIXME: MELHOR QUE SEJA "FIRE AND FORGET" POR ISSO EM OUTRA THREAD SE POSSÍVEL
                for(std::function<void()> callback : m_preparedRenderCallback) callback();

                for(SceneScopeSystem* system : m_systems) system->preRenderScene(deltaTime);

                renderScene();

                for(SceneScopeSystem* system : m_systems) system->postRenderScene(deltaTime);

                for(std::function<void()> callback : m_renderedCallback) callback();

                this->finishRender();
            }

            for(std::function<void()> callback : m_postRenderCallback) callback();

            return true;
        }

        void addSystem(SceneScopeSystem* system) { m_systems.push_back(system); }

        
        std::vector<std::function<void()>> m_preRenderCallback;

        std::vector<std::function<void()>> m_preparedRenderCallback;

        std::vector<std::function<void()>> m_renderedCallback;
        
        std::vector<std::function<void()>> m_postRenderCallback;
        
        UIRenderer* getCurrentUI()
        {
            return this->m_uiRenderer.get();
        }
        
    protected:
        
        virtual bool renderScene() = 0;
    
        std::unique_ptr<InstanceFactory> m_asset3dFactory;
        
        std::unique_ptr<UIRenderer> m_uiRenderer;
        
        std::map<int, Asset3DReference> m_3dInstances;


    private:

        int m_lastId = 0;
        std::vector<SceneScopeSystem*> m_systems;

    };
    
} 

