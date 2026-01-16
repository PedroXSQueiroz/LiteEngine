#pragma once

#include <core/lightning/IBL.h>

#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/IndirectLight.h>
#include <filament/Skybox.h>
#include <filament/Texture.h>

#include <math/vec3.h>

#include <string>

namespace lite {

class FilamentIBL : public IBL {
public:
    FilamentIBL(filament::Engine* engine, filament::Scene* scene);
    ~FilamentIBL();

    bool load(const std::string& path) override;

    // Getters for advanced usage
    filament::IndirectLight* getIndirectLight() const { return m_indirectLight; }
    filament::Skybox* getSkybox() const { return m_skybox; }
    filament::Texture* getFogTexture() const { return m_fogTexture; }
    bool hasSphericalHarmonics() const { return m_hasSphericalHarmonics; }
    filament::math::float3 const* getSphericalHarmonics() const { return m_bands; }

private:
    bool loadFromEquirect(const std::string& path);
    bool loadFromDirectory(const std::string& path);
    bool loadFromKtx(const std::string& prefix);

    bool loadCubemapLevel(
        filament::Texture** texture,
        const std::string& path,
        size_t level = 0,
        const std::string& levelPrefix = ""
    ) const;

    bool loadCubemapLevel(
        filament::Texture** texture,
        filament::Texture::PixelBufferDescriptor* outBuffer,
        uint32_t* dim,
        const std::string& path,
        size_t level = 0,
        const std::string& levelPrefix = ""
    ) const;

    filament::Engine* m_engine;
    filament::Scene* m_scene;

    filament::math::float3 m_bands[9] = {};
    bool m_hasSphericalHarmonics = false;

    filament::Texture* m_texture = nullptr;
    filament::IndirectLight* m_indirectLight = nullptr;
    filament::Texture* m_skyboxTexture = nullptr;
    filament::Texture* m_fogTexture = nullptr;
    filament::Skybox* m_skybox = nullptr;
};

} // namespace lite
