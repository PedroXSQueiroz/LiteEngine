#include <fstream>

#include <lite/services/SceneService.h>
#include <lite/services/Asset3dData.h>

#include <filament/Scene.h>
#include <filament/Engine.h>

using namespace std;
using namespace filament;

namespace lite{
    
    class Asset3dLoader : public SceneService
    {
    
    public:

        Asset3dLoader(
                filament::Engine* engine
            ,   filament::Scene* scene
        ):SceneService(engine, scene) {};

        virtual Asset3dData* load(std::string assetFilePath) = 0;
    };

}
