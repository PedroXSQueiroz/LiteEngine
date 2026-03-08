#pragma once

#include <memory>
#include <map>
#include <functional>

#include <core/concepts/EngineConcepts.h>
#include <core/assets/instanceFactory/Asset3dInstanceFactory.h>
#include <core/data/assets/Asset3dData.h>

using namespace std;


namespace lite
{
    template<
        Asset3dConcept AssetType, 
        TransformConcept TransformType, 
        Asset3dInstanceFactoryConcept InstanceFactory>
    class Scene {
        
        public:
        using Asset3DReference = std::unique_ptr<AssetType>;

        Scene(
            std::unique_ptr<InstanceFactory> asset3dFactory
        )
        : m_asset3dFactory(std::move(asset3dFactory)) {}

        int create(const Asset3dData& data, const std::vector<MaterialData>& materials, TransformType transform, AssetType*& instance) {
            Asset3DReference newInstance = this->m_asset3dFactory->instantiate(data, transform, materials);
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
            return m_3dInstances.erase(id) > 0;
        }

        bool destroy(Asset3DReference instance) {
            for (auto it = m_3dInstances.begin(); it != m_3dInstances.end(); ++it) {
                if (it->second.get() == instance.get()) {
                    m_3dInstances.erase(it);
                    return true;
                }
            }
            return false;
        }

        bool update()
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
            
            renderScene();
            
            for(std::function<void()> callback : m_postRenderCallback) callback();
        }

        virtual bool renderScene() = 0;
        
        std::vector<std::function<void()>> m_preRenderCallback;

        std::vector<std::function<void()>> m_postRenderCallback;

    protected:
    
        std::unique_ptr<InstanceFactory> m_asset3dFactory;
        
        std::map<int, Asset3DReference> m_3dInstances;

    private:

        int m_lastId = 0;

    };
    
} 

