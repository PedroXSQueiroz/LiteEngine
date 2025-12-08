cmake_minimum_required(VERSION 3.15)
project(lite_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Força runtime de debug /MTd (MultiThreadedDebug)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")


set(FILAMENT_ROOT "${CMAKE_SOURCE_DIR}/3rd_party/filament")
set(FILAMENT_LIBS_ROOT "${FILAMENT_ROOT}/out/libs")
set(FILAMENT_LIBS_INCLUDE "${FILAMENT_ROOT}/libs")
set(FILAMENT_LIBS_3rdPARTY "${FILAMENT_ROOT}/out/third_party/")
set(FILAMENT_LIBS_3rdPARTY_ROOT "${FILAMENT_ROOT}/third_party/")
set(FILAMENT_LIB_BUILD_VERSION "Debug")

set(SDL_ROOT "${CMAKE_SOURCE_DIR}/3rd_party/SDL-release-2.32.10")

# Encontrar Vulkan
find_package(Vulkan REQUIRED)

set(JOLT_ROOT "/3rd_party/JoltPhysics")
set(OZZ_ROOT "/3rd_party/ozz-animation")


set(FILAMENT_GENERATED_RESOURCES ${FILAMENT_ROOT}/out/filament/generated/resources)
set(FILAMENT_GENERATED_RESOURCES_MATERIALS ${FILAMENT_ROOT}/out/filament/generated/material)
set(FILAMENT_GENERATED_RESOURCES_DATA ${FILAMENT_ROOT}/out/filament/generated/data)
set(FILAMENT_GENERATED_RESOURCES_MATDBG ${FILAMENT_ROOT}/out/libs/matdbg)
set(FILAMENT_GENERATED_RESOURCES_GLTFIO_MATERIALS ${FILAMENT_ROOT}/out/libs/gltfio/materials)

add_executable(app 
  core/src/main.cpp
  core/src/lightning/IBL.cpp
)

#SETTING MATDBG RESOURCES
set(MATDBG_RESOURCES_SRC
    ${FILAMENT_GENERATED_RESOURCES_MATDBG}/matdbg_resources.c
    ${FILAMENT_GENERATED_RESOURCES_MATDBG}/matdbg_resources.h
)
set_source_files_properties(${MATDBG_RESOURCES_SRC} PROPERTIES LANGUAGE CXX)

add_library(matdbg_resources STATIC "${MATDBG_RESOURCES_SRC}")
target_include_directories(matdbg_resources PUBLIC ${FILAMENT_GENERATED_RESOURCES_MATDBG})
#END SETTING MATDBG RESOURCES

#SETTING MATDBG RESOURCES
set(UBER_ARCHIVE_RESOURCES_SRC
    ${FILAMENT_GENERATED_RESOURCES_GLTFIO_MATERIALS}/uberarchive.c
    ${FILAMENT_GENERATED_RESOURCES_GLTFIO_MATERIALS}/uberarchive.h
)
set_source_files_properties(${UBER_ARCHIVE_RESOURCES_SRC} PROPERTIES LANGUAGE CXX)

add_library(uber_archive STATIC "${UBER_ARCHIVE_RESOURCES_SRC}")
target_include_directories(uber_archive PUBLIC ${FILAMENT_GENERATED_RESOURCES_GLTFIO_MATERIALS})
#END SETTING MATDBG RESOURCES

include_directories(
  ${CMAKE_SOURCE_DIR}/core/src/include

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
  ${FILAMENT_LIBS_3rdPARTY_ROOT}/limbassimp
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

  ${Vulkan_INCLUDE_DIRS} 
)

target_link_libraries(app PUBLIC
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
    opengl32
    Shlwapi

    Vulkan::Vulkan 
)

target_sources(app PRIVATE 
  ${RESOURCE_SOURCES_MATDBG} 
  ${FILAMENT_ROOT}/out/libs/matdbg/matdbg_resources.c
  ${FILAMENT_ROOT}/out/libs/gltfio/materials/uberarchive.c
  
  # ${FILAMENT_LIBS_3rdPARTY_ROOT}/stb/stb_image.cpp
  # ${FILAMENT_LIBS_3rdPARTY_ROOT}/stb/stb_vorbis.c
)

# Garante consistência do runtime em todas as targets
set_property(TARGET app PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
add_subdirectory(${FILAMENT_LIBS_3rdPARTY_ROOT}/libassimp/contrib/zlib)