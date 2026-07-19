#pragma once

#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <algorithm>
#include <mutex>
#include <condition_variable>

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

        int create(const Asset3dData& data, const std::vector<MaterialData>& materials, TransformType transform) {
            std::lock_guard<std::mutex> lock(m_instancesMutex);
            m_creatingObjects.push_back({ ++m_lastId, data.clone(), materials, transform });
            return m_lastId;
        }

        AssetType* get(int id) {
            std::unique_lock<std::mutex> lock(m_instancesMutex);

            auto it = m_3dInstances.find(id);
            if (it != m_3dInstances.end()) return it->second.get();

            bool inQueue = std::any_of(m_creatingObjects.begin(), m_creatingObjects.end(),
                                       [id](const CreationEntry& e){ return e.id == id; });
            if (!inQueue) return nullptr;

            m_instantiatedCV.wait(lock, [&]{
                return m_3dInstances.contains(id);
            });

            return m_3dInstances.at(id).get();
        }

        std::vector<AssetType*> find(std::function<bool(AssetType*)> criteria) {
            std::lock_guard<std::mutex> lock(m_instancesMutex);
            std::vector<AssetType*> result;
            for (const auto& [key, value] : m_3dInstances) {
                if (criteria(value.get())) result.push_back(value.get());
            }
            return result;
        }

        bool destroy(int id) {
            std::lock_guard<std::mutex> lock(m_instancesMutex);
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
            std::lock_guard<std::mutex> lock(m_instancesMutex);
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
            */

            instantiate();

            for(SceneScopeSystem* system : m_systems) system->onFrameBegin(deltaTime);

            if(this->m_uiRenderer) this->m_uiRenderer->update();

            if(this->prepareRender())
            {
                for(SceneScopeSystem* system : m_systems) system->onRenderPrepared(deltaTime);

                for(SceneScopeSystem* system : m_systems) system->preRenderScene(deltaTime);

                renderScene();

                for(SceneScopeSystem* system : m_systems) system->postRenderScene(deltaTime);

                this->renderUI();

                for(SceneScopeSystem* system : m_systems) system->onSceneRendered(deltaTime);

                this->finishRender();
            }

            for(SceneScopeSystem* system : m_systems) system->onFrameEnd(deltaTime);

            return true;
        }

        // THREADING: registrar/remover antes de SceneRenderer::start() (main
        // thread) ou via postCommand (render thread) — o vetor não tem lock.
        void addSystem(SceneScopeSystem* system) { m_systems.push_back(system); }

        void removeSystem(SceneScopeSystem* system) {
            m_systems.erase(
                std::remove(m_systems.begin(), m_systems.end(), system),
                m_systems.end()
            );
        }

        UIRenderer* getCurrentUI()
        {
            return this->m_uiRenderer.get();
        }

    protected:

        virtual bool renderScene() = 0;

        // Composição da UI por cima da cena, dentro do frame aberto. A base não
        // conhece o renderer gráfico concreto — a implementação (ex.:
        // FilamentScene) chama m_uiRenderer->render(rendererConcreto).
        virtual void renderUI() {}

        std::unique_ptr<InstanceFactory> m_asset3dFactory;

        std::unique_ptr<UIRenderer> m_uiRenderer;

        std::map<int, Asset3DReference> m_3dInstances;


    private:

        struct CreationEntry {
            int id;
            std::unique_ptr<Asset3dData> data;
            std::vector<MaterialData> materials;
            TransformType transform;
        };

        void instantiate() {
            std::vector<CreationEntry> batch;
            {
                std::lock_guard<std::mutex> lock(m_instancesMutex);
                batch = std::move(m_creatingObjects);
            }

            for (auto& entry : batch) {
                Asset3DReference instance = m_asset3dFactory->instantiateAsset(*entry.data, entry.transform, entry.materials);
                {
                    std::lock_guard<std::mutex> lock(m_instancesMutex);
                    m_3dInstances.emplace(entry.id, std::move(instance));
                }
                m_instantiatedCV.notify_all();
            }
        }

        int m_lastId = 0;
        std::vector<SceneScopeSystem*> m_systems;
        std::vector<CreationEntry> m_creatingObjects;
        std::mutex m_instancesMutex;
        std::condition_variable m_instantiatedCV;
    };

}
