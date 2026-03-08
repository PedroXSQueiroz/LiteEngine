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
    template<Asset3dConcept AssetType, TransformConcept TransformType>
    class Scene {
        
        public:
        using Asset3DReference = std::unique_ptr<AssetType>;

        Scene(std::unique_ptr<Asset3dInstanceFactory<AssetType, TransformType>> asset3dFactory)
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

    private:
        
        int m_lastId = 0;

        std::unique_ptr<Asset3dInstanceFactory<AssetType, TransformType>> m_asset3dFactory;

        std::map<int, Asset3DReference> m_3dInstances;

    };
    
} 

