#include <filament/lightning/FilamentIBL.h>

#include <filament/Engine.h>
#include <filament/IndirectLight.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Skybox.h>
#include <filament/Texture.h>

#include <ktxreader/Ktx1Reader.h>

#include <imageio/ImageDecoder.h>

#include <filament-iblprefilter/IBLPrefilterContext.h>

#include <stb_image.h>

#include <utils/Path.h>

#include <fstream>
#include <iostream>
#include <string>
#include <cstring>

using namespace filament;
using namespace filament::math;
using namespace ktxreader;
using namespace utils;

namespace lite {

static constexpr float IBL_INTENSITY = 30000.0f;

FilamentIBL::FilamentIBL(filament::Engine* engine, filament::Scene* scene)
    : m_engine(engine)
    , m_scene(scene)
{
}

FilamentIBL::~FilamentIBL() {
    if (m_engine) {
        m_engine->destroy(m_indirectLight);
        m_engine->destroy(m_texture);
        m_engine->destroy(m_skybox);
        m_engine->destroy(m_skyboxTexture);
        m_engine->destroy(m_fogTexture);
    }
}

bool FilamentIBL::load(const std::string& path) {
    Path iblPath(path);
    if (!iblPath.exists()) {
        std::cerr << "FilamentIBL: Path does not exist: " << path << std::endl;
        return false;
    }

    bool success = false;
    if (iblPath.isDirectory()) {
        success = loadFromDirectory(path);
    } else {
        success = loadFromEquirect(path);
    }

    if (success) {
        m_scene->setSkybox(m_skybox);
        m_scene->setIndirectLight(m_indirectLight);
        std::cout << "FilamentIBL: Loaded successfully from " << path << std::endl;
    }

    return success;
}

bool FilamentIBL::loadFromEquirect(const std::string& pathStr) {
    Path path(pathStr);
    if (!path.exists()) {
        return false;
    }

    int w = 0, h = 0;
    int n = 0;
    size_t size = 0;
    void* data = nullptr;
    void* user = nullptr;
    Texture::PixelBufferDescriptor::Callback destroyer{};

    if (path.getExtension() == "exr") {
        std::ifstream in_stream(path.getAbsolutePath().c_str(), std::ios::binary);
        image::LinearImage* image = new image::LinearImage(
                image::ImageDecoder::decode(in_stream, path.getAbsolutePath().c_str()));
        w = image->getWidth();
        h = image->getHeight();
        n = image->getChannels();
        size = w * h * n * sizeof(float);
        data = image->getPixelRef();
        user = image;
        destroyer = [](void*, size_t, void* user) {
            delete reinterpret_cast<image::LinearImage*>(user);
        };
    } else {
        stbi_info(path.getAbsolutePath().c_str(), &w, &h, nullptr);
        // load image as float
        size = w * h * sizeof(float3);
        data = (float3*)stbi_loadf(path.getAbsolutePath().c_str(), &w, &h, &n, 3);
        destroyer = [](void* data, size_t, void*) {
            stbi_image_free(data);
        };
    }

    if (data == nullptr || n != 3) {
        std::cerr << "FilamentIBL: Could not decode image" << std::endl;
        destroyer(data, size, user);
        return false;
    }

    if (w != h * 2) {
        std::cerr << "FilamentIBL: Not an equirectangular image!" << std::endl;
        destroyer(data, size, user);
        return false;
    }

    // now load texture
    Texture::PixelBufferDescriptor buffer(
            data, size, Texture::Format::RGB, Texture::Type::FLOAT, destroyer, user);

    Texture* const equirect = Texture::Builder()
            .width((uint32_t)w)
            .height((uint32_t)h)
            .levels(0xff)
            .format(Texture::InternalFormat::R11F_G11F_B10F)
            .sampler(Texture::Sampler::SAMPLER_2D)
            .usage(Texture::Usage::DEFAULT | Texture::Usage::GEN_MIPMAPPABLE)
            .build(*m_engine);

    equirect->setImage(*m_engine, 0, std::move(buffer));

    IBLPrefilterContext context(*m_engine);
    IBLPrefilterContext::EquirectangularToCubemap equirectangularToCubemap(context);
    IBLPrefilterContext::SpecularFilter specularFilter(context);
    IBLPrefilterContext::IrradianceFilter irradianceFilter(context);

    m_skyboxTexture = equirectangularToCubemap(equirect);

    m_engine->destroy(equirect);

    m_texture = specularFilter(m_skyboxTexture);

    m_fogTexture = irradianceFilter({ .generateMipmap = true }, m_skyboxTexture);
    m_fogTexture->generateMipmaps(*m_engine);

    m_indirectLight = IndirectLight::Builder()
            .reflections(m_texture)
            .intensity(IBL_INTENSITY)
            .build(*m_engine);

    m_skybox = Skybox::Builder()
            .environment(m_skyboxTexture)
            .showSun(true)
            .build(*m_engine);

    return true;
}

bool FilamentIBL::loadFromKtx(const std::string& prefix) {
    Path iblPath(prefix + "_ibl.ktx");
    if (!iblPath.exists()) {
        return false;
    }
    Path skyPath(prefix + "_skybox.ktx");
    if (!skyPath.exists()) {
        return false;
    }

    auto createKtx = [] (Path path) {
        using namespace std;
        ifstream file(path.getPath(), ios::binary);
        vector<uint8_t> contents((istreambuf_iterator<char>(file)), {});
        return new image::Ktx1Bundle(contents.data(), contents.size());
    };

    Ktx1Bundle* iblKtx = createKtx(iblPath);
    Ktx1Bundle* skyKtx = createKtx(skyPath);

    m_skyboxTexture = Ktx1Reader::createTexture(m_engine, skyKtx, false);
    m_texture = Ktx1Reader::createTexture(m_engine, iblKtx, false);

    if (!iblKtx->getSphericalHarmonics(m_bands)) {
        return false;
    }
    m_hasSphericalHarmonics = true;

    m_indirectLight = IndirectLight::Builder()
            .reflections(m_texture)
            .intensity(IBL_INTENSITY)
            .build(*m_engine);

    m_skybox = Skybox::Builder().environment(m_skyboxTexture).showSun(true).build(*m_engine);

    return true;
}

bool FilamentIBL::loadFromDirectory(const std::string& pathStr) {
    Path path(pathStr);

    // First check if KTX files are available.
    if (loadFromKtx(Path::concat(path, path.getName()))) {
        return true;
    }

    // Read spherical harmonics
    Path sh(Path::concat(path, "sh.txt"));
    if (sh.exists()) {
        std::ifstream shReader(sh);
        shReader >> std::skipws;

        std::string line;
        for (float3& band : m_bands) {
            std::getline(shReader, line);
            int n = sscanf(line.c_str(), "(%f,%f,%f)", &band.r, &band.g, &band.b);
            if (n != 3) return false;
        }
    } else {
        return false;
    }
    m_hasSphericalHarmonics = true;

    // Read mip-mapped cubemap
    const std::string prefix = "m";
    if (!loadCubemapLevel(&m_texture, pathStr, 0, prefix + "0_")) return false;

    size_t numLevels = m_texture->getLevels();
    for (size_t i = 1; i < numLevels; i++) {
        const std::string levelPrefix = prefix + std::to_string(i) + "_";
        loadCubemapLevel(&m_texture, pathStr, i, levelPrefix);
    }

    if (!loadCubemapLevel(&m_skyboxTexture, pathStr)) return false;

    m_indirectLight = IndirectLight::Builder()
            .reflections(m_texture)
            .irradiance(3, m_bands)
            .intensity(IBL_INTENSITY)
            .build(*m_engine);

    m_skybox = Skybox::Builder().environment(m_skyboxTexture).showSun(true).build(*m_engine);

    return true;
}

bool FilamentIBL::loadCubemapLevel(
    filament::Texture** texture,
    const std::string& pathStr,
    size_t level,
    const std::string& levelPrefix
) const {
    uint32_t dim;
    Texture::PixelBufferDescriptor buffer;
    if (loadCubemapLevel(texture, &buffer, &dim, pathStr, level, levelPrefix)) {
        (*texture)->setImage(*m_engine, level, 0, 0, 0, dim, dim, 6, std::move(buffer));
        return true;
    }
    return false;
}

bool FilamentIBL::loadCubemapLevel(
    filament::Texture** texture,
    Texture::PixelBufferDescriptor* outBuffer,
    uint32_t* dim,
    const std::string& pathStr,
    size_t level,
    const std::string& levelPrefix
) const {
    Path path(pathStr);
    static const char* faceSuffix[6] = { "px", "nx", "py", "ny", "pz", "nz" };

    size_t size = 0;
    size_t numLevels = 1;

    { // this is just a scope to avoid variable name hiding below
        int w, h;
        std::string faceName = levelPrefix + faceSuffix[0] + ".rgb32f";
        Path facePath(Path::concat(path, faceName));
        if (!facePath.exists()) {
            std::cerr << "FilamentIBL: The face " << faceName << " does not exist" << std::endl;
            return false;
        }
        stbi_info(facePath.getAbsolutePath().c_str(), &w, &h, nullptr);
        if (w != h) {
            std::cerr << "FilamentIBL: width != height" << std::endl;
            return false;
        }

        size = (size_t)w;

        if (!levelPrefix.empty()) {
            numLevels = (size_t)std::log2(size) + 1;
        }

        if (level == 0) {
            *texture = Texture::Builder()
                    .width((uint32_t)size)
                    .height((uint32_t)size)
                    .levels((uint8_t)numLevels)
                    .format(Texture::InternalFormat::R11F_G11F_B10F)
                    .sampler(Texture::Sampler::SAMPLER_CUBEMAP)
                    .build(*m_engine);
        }
    }

    // RGB_10_11_11_REV encoding: 4 bytes per pixel
    const size_t faceSize = size * size * sizeof(uint32_t);
    *dim = size;

    Texture::PixelBufferDescriptor buffer(
            malloc(faceSize * 6), faceSize * 6,
            Texture::Format::RGB, Texture::Type::UINT_10F_11F_11F_REV,
            (Texture::PixelBufferDescriptor::Callback) &free);

    bool success = true;
    uint8_t* p = static_cast<uint8_t*>(buffer.buffer);

    for (size_t j = 0; j < 6; j++) {
        std::string faceName = levelPrefix + faceSuffix[j] + ".rgb32f";
        Path facePath(Path::concat(path, faceName));
        if (!facePath.exists()) {
            std::cerr << "FilamentIBL: The face " << faceName << " does not exist" << std::endl;
            success = false;
            break;
        }

        int w, h, n;
        unsigned char* data = stbi_load(facePath.getAbsolutePath().c_str(), &w, &h, &n, 4);
        if (w != h || w != size) {
            std::cerr << "FilamentIBL: Face " << faceName << " has a wrong size " << w << " x " << h
                      << ", instead of " << size << " x " << size << std::endl;
            success = false;
            break;
        }

        if (data == nullptr || n != 4) {
            std::cerr << "FilamentIBL: Could not decode face " << faceName << std::endl;
            success = false;
            break;
        }

        memcpy(p + faceSize * j, data, w * h * sizeof(uint32_t));

        stbi_image_free(data);
    }

    if (!success) return false;

    *outBuffer = std::move(buffer);

    return true;
}

} // namespace lite
