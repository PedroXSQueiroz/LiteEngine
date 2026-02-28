#pragma once

#include <filament/Engine.h>

class FilamentUtils {

public:

    static void setEngine(filament::Engine* engine) {
        m_currentEngine = engine;
    }

    static filament::Engine* getEngine() {
        return m_currentEngine;
    }

private:

    static filament::Engine* m_currentEngine;

};