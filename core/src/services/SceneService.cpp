#include <SceneService.h>
#include <filament/Scene.h>
#include <filament/Engine.h>

using namespace lite;
using namespace filament;

filament::Scene* SceneService::getScene(){
    return this->m_scene;
}

filament::Engine* SceneService::getEngine(){
    return this->m_engine;
}