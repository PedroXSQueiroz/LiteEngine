cmake_minimum_required(VERSION 3.15)
project(lite_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# ==================== PATHS EXISTENTES ====================
set(FILAMENT_ROOT "${CMAKE_SOURCE_DIR}/3rd_party/filament")
set(FILAMENT_LIBS_ROOT "${FILAMENT_ROOT}/out/libs")
set(FILAMENT_LIBS_INCLUDE "${FILAMENT_ROOT}/libs")
set(FILAMENT_LIBS_3rdPARTY "${FILAMENT_ROOT}/out/third_party/")
set(FILAMENT_LIBS_3rdPARTY_ROOT "${FILAMENT_ROOT}/third_party/")
set(FILAMENT_LIB_BUILD_VERSION "Debug")

set(SDL_ROOT "${CMAKE_SOURCE_DIR}/3rd_party/SDL-release-2.32.10")
find_package(Vulkan REQUIRED)

set(JOLT_ROOT "/3rd_party/JoltPhysics")
set(OZZ_ROOT "/3rd_party/ozz-animation")


# GLM - Header only math library
set(GLM_ROOT "${CMAKE_SOURCE_DIR}/3rd_party/glm")

# ==================== CEF CONFIGURATION ====================
# IMPORTANTE: Definir CEF_ROOT ANTES de find_package
set(CEF_ROOT "${CMAKE_SOURCE_DIR}/3rd_party/cef")

# Adicionar pasta cmake local ao module path
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

# Agora pode usar find_package
find_package(CEF REQUIRED)

# ==================== FILAMENT RESOURCES ====================
set(FILAMENT_GENERATED_RESOURCES ${FILAMENT_ROOT}/out/filament/generated/resources)
set(FILAMENT_GENERATED_RESOURCES_MATERIALS ${FILAMENT_ROOT}/out/filament/generated/material)
set(FILAMENT_GENERATED_RESOURCES_DATA ${FILAMENT_ROOT}/out/filament/generated/data)
set(FILAMENT_GENERATED_RESOURCES_MATDBG ${FILAMENT_ROOT}/out/libs/matdbg)
set(FILAMENT_GENERATED_RESOURCES_GLTFIO_MATERIALS ${FILAMENT_ROOT}/out/libs/gltfio/materials)

# ==================== LIBCEF_DLL_WRAPPER ====================
# Adicionar subdirectory do libcef_dll_wrapper
add_subdirectory(${CEF_ROOT}/libcef_dll libcef_dll_wrapper)

# ==================== EXECUTÁVEL PRINCIPAL ====================
add_executable(app 
  core/src/main.cpp
  
)

# ==================== MATDBG RESOURCES ====================
set(MATDBG_RESOURCES_SRC
    ${FILAMENT_GENERATED_RESOURCES_MATDBG}/matdbg_resources.c
    ${FILAMENT_GENERATED_RESOURCES_MATDBG}/matdbg_resources.h
)
set_source_files_properties(${MATDBG_RESOURCES_SRC} PROPERTIES LANGUAGE CXX)
add_library(matdbg_resources STATIC "${MATDBG_RESOURCES_SRC}")
target_include_directories(matdbg_resources PUBLIC ${FILAMENT_GENERATED_RESOURCES_MATDBG})

# ==================== UBER ARCHIVE ====================
set(UBER_ARCHIVE_RESOURCES_SRC
    ${FILAMENT_GENERATED_RESOURCES_GLTFIO_MATERIALS}/uberarchive.c
    ${FILAMENT_GENERATED_RESOURCES_GLTFIO_MATERIALS}/uberarchive.h
)
set_source_files_properties(${UBER_ARCHIVE_RESOURCES_SRC} PROPERTIES LANGUAGE CXX)
add_library(uber_archive STATIC "${UBER_ARCHIVE_RESOURCES_SRC}")
target_include_directories(uber_archive PUBLIC ${FILAMENT_GENERATED_RESOURCES_GLTFIO_MATERIALS})

# ==================== INCLUDE DIRECTORIES ====================
target_include_directories(app PRIVATE
  # Agnostic architecture headers
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/core/src/include
  ${CMAKE_SOURCE_DIR}/core/src/include/lite/cef
  ${CMAKE_SOURCE_DIR}/core/src/include/lite/ui

  # CEF
  ${CEF_INCLUDE_PATH}

  # FILAMENT
  ${FILAMENT_ROOT}/filament/include
  ${FILAMENT_ROOT}/filament/backend/include
  ${FILAMENT_LIBS_INCLUDE}/bluegl/include
  ${FILAMENT_LIBS_INCLUDE}/bluevk/include 
  ${FILAMENT_LIBS_INCLUDE}/camutils/include
  ${FILAMENT_LIBS_INCLUDE}/filabridge/include
  ${FILAMENT_LIBS_INCLUDE}/filaflat/include
  ${FILAMENT_LIBS_INCLUDE}/filagui/include
  ${FILAMENT_LIBS_INCLUDE}/filamat/include
  ${FILAMENT_LIBS_INCLUDE}/filament-matp/include
  ${FILAMENT_LIBS_INCLUDE}/filamentapp/include
  ${FILAMENT_LIBS_INCLUDE}/filameshio/include
  ${FILAMENT_LIBS_INCLUDE}/generatePrefilterMipmap/include
  ${FILAMENT_LIBS_INCLUDE}/geometry/include
  ${FILAMENT_LIBS_INCLUDE}/gltfio/include
  ${FILAMENT_LIBS_ROOT}/gltfio/materials
  ${FILAMENT_LIBS_INCLUDE}/ibl/include
  ${FILAMENT_LIBS_INCLUDE}/iblprefilter/include
  ${FILAMENT_LIBS_INCLUDE}/image/include
  ${FILAMENT_LIBS_INCLUDE}/imageio/include
  ${FILAMENT_LIBS_INCLUDE}/ktxreader/include
  ${FILAMENT_LIBS_INCLUDE}/matdbg/include
  ${FILAMENT_LIBS_INCLUDE}/math/include
  ${FILAMENT_LIBS_INCLUDE}/mathio/include
  ${FILAMENT_LIBS_INCLUDE}/uberz/include
  ${FILAMENT_LIBS_INCLUDE}/utils/include
  ${FILAMENT_LIBS_INCLUDE}/viewer/include
  ${FILAMENT_LIBS_INCLUDE}/mikktspace/include
  
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/stb
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/libpng
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/tinyexr
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/libassimp/include
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/libz
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/robin-map
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/meshoptimizer/src
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/draco/src
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/basisu/encoder/src
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/basisu/transcoder/src
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/basisu/zstd/src

  ${JOLT_ROOT}/include
  ${OZZ_ROOT}/include
  ${SDL_ROOT}/include
  ${GLM_ROOT}

  # nlohmann/json - Header only JSON library
  ${CMAKE_SOURCE_DIR}/3rd_party/nlohmann-json/include

  ${Vulkan_INCLUDE_DIRS}
)

# ==================== LINK LIBRARIES ====================
target_link_libraries(app PRIVATE
    # CEF - IMPORTANTE: libcef_dll_wrapper ANTES de libcef.lib
    libcef_dll_wrapper
    ${CEF_LIB_DEBUG}

    # FILAMENT (suas libs existentes)
    "${FILAMENT_ROOT}/out/filament/Debug/filament.lib"
    "${FILAMENT_ROOT}/out/filament/backend/Debug/backend.lib"
    "${FILAMENT_LIBS_ROOT}/bluevk/Debug/bluevk.lib"
    "${FILAMENT_LIBS_ROOT}/gltfio/Debug/gltfio.lib"
    "${FILAMENT_LIBS_ROOT}/gltfio/Debug/gltfio_core.lib"
    "${FILAMENT_LIBS_ROOT}/gltfio/Debug/uberarchive.lib"
    "${FILAMENT_LIBS_ROOT}/utils/Debug/utils.lib"
    "${FILAMENT_LIBS_ROOT}/math/Debug/math.lib"
    "${SDL_ROOT}/build/Debug/SDL2d.lib"
    "${SDL_ROOT}/build/Debug/SDL2maind.lib"
    "${FILAMENT_LIBS_ROOT}/bluegl/Debug/bluegl.lib"
    "${FILAMENT_LIBS_ROOT}/camutils/Debug/camutils.lib"
    "${FILAMENT_LIBS_ROOT}/filabridge/Debug/filabridge.lib"
    "${FILAMENT_LIBS_ROOT}/filaflat/Debug/filaflat.lib"
    "${FILAMENT_LIBS_ROOT}/filagui/Debug/filagui.lib"
    "${FILAMENT_LIBS_ROOT}/filamat/Debug/filamat.lib"
    "${FILAMENT_LIBS_ROOT}/filamentapp/Debug/filamentapp.lib"
    "${FILAMENT_LIBS_ROOT}/filameshio/Debug/filameshio.lib"
    "${FILAMENT_LIBS_ROOT}/generatePrefilterMipmap/Debug/filament-generatePrefilterMipmap.lib"
    "${FILAMENT_LIBS_ROOT}/geometry/Debug/geometry.lib"
    "${FILAMENT_LIBS_ROOT}/ibl/Debug/ibl.lib"
    "${FILAMENT_LIBS_ROOT}/iblprefilter/Debug/filament-iblprefilter.lib"
    "${FILAMENT_LIBS_ROOT}/image/Debug/image.lib"
    "${FILAMENT_LIBS_ROOT}/imageio/Debug/imageio.lib"
    "${FILAMENT_LIBS_ROOT}/ktxreader/Debug/ktxreader.lib"
    "${FILAMENT_LIBS_ROOT}/matdbg/Debug/matdbg.lib"
    "${FILAMENT_LIBS_ROOT}/matdbg/Debug/matdbg_resources.lib"
    "${FILAMENT_LIBS_ROOT}/mathio/Debug/mathio.lib"
    "${FILAMENT_LIBS_ROOT}/uberz/Debug/uberzlib.lib"
    "${FILAMENT_LIBS_ROOT}/viewer/Debug/viewer.lib"
    
    "${FILAMENT_LIBS_3rdPARTY}/zstd/tnt/Debug/zstd.lib"
    "${FILAMENT_LIBS_3rdPARTY}/civetweb/tnt/Debug/civetweb.lib"
    "${FILAMENT_LIBS_3rdPARTY}/glslang/tnt/glslang/Debug/glslangd.lib"
    "${FILAMENT_LIBS_3rdPARTY}/glslang/tnt/SPIRV/Debug/SPIRVd.lib"
    "${FILAMENT_LIBS_3rdPARTY}/spirv-cross/tnt/Debug/spirv-cross-core.lib"
    "${FILAMENT_LIBS_3rdPARTY}/spirv-cross/tnt/Debug/spirv-cross-glsl.lib"
    "${FILAMENT_LIBS_3rdPARTY}/spirv-cross/tnt/Debug/spirv-cross-msl.lib"
    "${FILAMENT_LIBS_3rdPARTY}/spirv-tools/source/Debug/SPIRV-tools.lib"
    "${FILAMENT_LIBS_3rdPARTY}/stb/tnt/Debug/stb.lib"
    "${FILAMENT_LIBS_3rdPARTY}/libpng/tnt/Debug/png.lib"
    "${FILAMENT_LIBS_3rdPARTY}/tinyexr/tnt/Debug/tinyexr.lib"
    "${FILAMENT_LIBS_3rdPARTY}/libassimp/tnt/Debug/assimp.lib"
    "${FILAMENT_LIBS_3rdPARTY}/libz/tnt/Debug/z.lib" 
    "${FILAMENT_LIBS_3rdPARTY}/smol-v/tnt/Debug/smol-v.lib"
    "${FILAMENT_LIBS_3rdPARTY}/meshoptimizer/tnt/Debug/meshoptimizer.lib"
    "${FILAMENT_LIBS_3rdPARTY}/draco/tnt/Debug/dracodec.lib"
    "${FILAMENT_LIBS_3rdPARTY}/mikktspace/Debug/mikktspace.lib"
    "${FILAMENT_LIBS_3rdPARTY}/basisu/tnt/Debug/basis_encoder.lib"
    "${FILAMENT_LIBS_3rdPARTY}/basisu/tnt/Debug/basis_transcoder.lib"
    "${FILAMENT_LIBS_3rdPARTY}/basisu/tnt/Debug/zstd-basisu.lib"
    
    # Windows libs
    opengl32
    Shlwapi
    comctl32
    rpcrt4
    
    Vulkan::Vulkan 
)

# ==================== SOURCES ====================
target_sources(app PRIVATE
  ${RESOURCE_SOURCES_MATDBG}
  ${FILAMENT_ROOT}/out/libs/matdbg/matdbg_resources.c
  ${FILAMENT_ROOT}/out/libs/gltfio/materials/uberarchive.c

  # ${CMAKE_SOURCE_DIR}/core/src/services/SceneService.cpp
  # ${CMAKE_SOURCE_DIR}/core/src/services/Asset3dData.cpp
  # ${CMAKE_SOURCE_DIR}/core/src/services/Asset3dLoaderAssimp.cpp
  # ${CMAKE_SOURCE_DIR}/core/src/services/MaterialLoader.cpp

  # ${CMAKE_SOURCE_DIR}/core/src/services/SceneService.cpp
  # ${CMAKE_SOURCE_DIR}/core/src/services/Asset3dData.cpp
  # ${CMAKE_SOURCE_DIR}/core/src/services/Asset3dLoaderAssimp.cpp
  # ${CMAKE_SOURCE_DIR}/core/src/services/MaterialLoader.cpp

  # Agnostic architecture
  ${CMAKE_SOURCE_DIR}/assimp/assets/importer/AssimpImporter.cpp
  ${CMAKE_SOURCE_DIR}/filament/assets/instanceFactory/FilamentInstanceFactory.cpp
  ${CMAKE_SOURCE_DIR}/filament/assets/instanceFactory/FilamentAsset3dInstance.cpp
  ${CMAKE_SOURCE_DIR}/filament/assets/instanceFactory/FilamentMeshAsset3dInstance.cpp
  ${CMAKE_SOURCE_DIR}/filament/data/assets/FilamentAsset3dTransform.cpp
  ${CMAKE_SOURCE_DIR}/filament/data/assets/FilamentCameraAsset3dInstance.cpp
  ${CMAKE_SOURCE_DIR}/filament/lightning/FilamentIBL.cpp
  ${CMAKE_SOURCE_DIR}/filament/editor/FilamentWireframeSystem.cpp
  ${CMAKE_SOURCE_DIR}/filament/utils/FilamentTransformUtils.cpp
  ${CMAKE_SOURCE_DIR}/filament/utils/FilamentUtils.cpp

  # UI
  ${CMAKE_SOURCE_DIR}/CEF/ui/CEF_Filament_UIRendererThreaded.cpp
  ${CMAKE_SOURCE_DIR}/CEF/ui/elements/CEF_UIElements.cpp
  ${CMAKE_SOURCE_DIR}/CEF/CEF_UIApp.cpp
  ${CMAKE_SOURCE_DIR}/CEF/ui/CEF_UIRenderProcessHandler.cpp

  # Core
  ${CMAKE_SOURCE_DIR}/core/src/data/assets/Asset3dData.cpp
  ${CMAKE_SOURCE_DIR}/core/src/data/assets/Asset3dInstance.cpp
  ${CMAKE_SOURCE_DIR}/core/src/data/assets/Asset3dTransform.cpp
  ${CMAKE_SOURCE_DIR}/core/src/input/InputEnums.cpp
  ${CMAKE_SOURCE_DIR}/core/src/scene/Scene.cpp

)

# Garante consistência do runtime em todas as targets
set_property(TARGET app PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
set_property(TARGET libcef_dll_wrapper PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
# set_property(TARGET app_helper PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")

add_subdirectory(${FILAMENT_LIBS_3rdPARTY_ROOT}/libassimp/contrib/zlib)