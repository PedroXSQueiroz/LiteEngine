#include <filament/Scene.h>
#include <filament/Engine.h>

using namespace filament;

namespace lite{
    
    class SceneService {
    
        public:
    
            SceneService(
                filament::Engine* engine, 
                filament::Scene* scene
            ): m_engine(engine), m_scene(scene) {};
    
        
        protected:
    
            filament::Engine* getEngine();
            filament::Scene* getScene();
    
        private:
    
            filament::Engine* m_engine;
            filament::Scene* m_scene;
    
    };

}