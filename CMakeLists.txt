cmake_minimum_required(VERSION 3.15)
project(filament_jolt_ozz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Força runtime de debug /MTd (MultiThreadedDebug)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# Garante que o nível de debug do iterador é compatível com MTd
# add_compile_definitions(
#     _ITERATOR_DEBUG_LEVEL=0
#     _HAS_ITERATOR_DEBUGGING=1
# )

set(FILAMENT_ROOT "${CMAKE_SOURCE_DIR}/3rd_party/filament")
set(FILAMENT_LIBS_ROOT "${FILAMENT_ROOT}/out/libs")
set(FILAMENT_LIBS_INCLUDE "${FILAMENT_ROOT}/libs")
set(FILAMENT_LIBS_3rdPARTY "${FILAMENT_ROOT}/out/third_party/")
set(FILAMENT_LIB_BUILD_VERSION "Debug")

set(SDL3_ROOT "${CMAKE_SOURCE_DIR}/3rd_party/SDL-release-3.2.24")

set(JOLT_ROOT "/3rd_party/JoltPhysics")
set(OZZ_ROOT "/3rd_party/ozz-animation")


set(FILAMENT_GENERATED_RESOURCES ${FILAMENT_ROOT}/out/filament/generated/resources)
set(FILAMENT_GENERATED_RESOURCES_MATERIALS ${FILAMENT_ROOT}/out/filament/generated/material)
set(FILAMENT_GENERATED_RESOURCES_DATA ${FILAMENT_ROOT}/out/filament/generated/data)
set(FILAMENT_GENERATED_RESOURCES_MATDBG ${FILAMENT_ROOT}/out/libs/matdbg)

add_executable(app 
  core/src/main.cpp
)

# file(GLOB RESOURCE_SOURCES_MATDBG 
#           # "${FILAMENT_GENERATED_RESOURCES}/*.c"
#           # "${FILAMENT_GENERATED_RESOURCES}/*.h"
#           # "${FILAMENT_GENERATED_RESOURCES}/*.bin"
#           # "${FILAMENT_GENERATED_RESOURCES}/*.S"
          
#           # "${FILAMENT_GENERATED_RESOURCES_MATERIALS}/*.filamat"

#           # "${FILAMENT_GENERATED_RESOURCES_DATA}/*.bin"

#           "${FILAMENT_GENERATED_RESOURCES_MATDBG}/*.c"
#           "${FILAMENT_GENERATED_RESOURCES_MATDBG}/*.h"
#           # "${FILAMENT_GENERATED_RESOURCES_MATDBG}/*.bin"
# )

#SETTING MATDBG RESOURCES
set(MATDBG_RESOURCES_SRC
    ${FILAMENT_GENERATED_RESOURCES_MATDBG}/matdbg_resources.c
    ${FILAMENT_GENERATED_RESOURCES_MATDBG}/matdbg_resources.h
)
set_source_files_properties(${MATDBG_RESOURCES_SRC} PROPERTIES LANGUAGE CXX)

add_library(matdbg_resources STATIC "${MATDBG_RESOURCES_SRC}")
target_include_directories(matdbg_resources PUBLIC ${FILAMENT_GENERATED_RESOURCES_MATDBG})
#END SETTING MATDBG RESOURCES

include_directories(
  ${FILAMENT_ROOT}/filament/include
  ${FILAMENT_ROOT}/filament/backend/include

  ${FILAMENT_LIBS_INCLUDE}/bluegl/include
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

  ${JOLT_ROOT}/include
  ${OZZ_ROOT}/include
  ${SDL3_ROOT}/include
)

link_directories(
  ${FILAMENT_ROOT}/out/filament/Debug
  ${FILAMENT_ROOT}/out/filament/backend/Debug
  ${FILAMENT_LIBS_ROOT}/bluegl/Debug
  ${FILAMENT_LIBS_ROOT}/camutils/Debug
  ${FILAMENT_LIBS_ROOT}/filabridge/Debug
  ${FILAMENT_LIBS_ROOT}/filaflat/Debug
  ${FILAMENT_LIBS_ROOT}/filagui/Debug
  ${FILAMENT_LIBS_ROOT}/filamat/Debug
  ${FILAMENT_LIBS_ROOT}/filament-matp/Debug
  ${FILAMENT_LIBS_ROOT}/filamentapp/Debug
  ${FILAMENT_LIBS_ROOT}/filameshio/Debug
  ${FILAMENT_LIBS_ROOT}/generatePrefilterMipmap/Debug
  ${FILAMENT_LIBS_ROOT}/geometry/Debug
  ${FILAMENT_LIBS_ROOT}/gltfio/Debug
  ${FILAMENT_LIBS_ROOT}/ibl/Debug
  ${FILAMENT_LIBS_ROOT}/iblprefilter/Debug
  ${FILAMENT_LIBS_ROOT}/image/Debug
  ${FILAMENT_LIBS_ROOT}/imageio/Debug
  ${FILAMENT_LIBS_ROOT}/ktxreader/Debug
  ${FILAMENT_LIBS_ROOT}/matdbg/Debug
  ${FILAMENT_LIBS_ROOT}/math/Debug
  ${FILAMENT_LIBS_ROOT}/mathio/Debug
  ${FILAMENT_LIBS_ROOT}/uberz/Debug
  ${FILAMENT_LIBS_ROOT}/utils/Debug
  ${FILAMENT_LIBS_ROOT}/viewer/Debug

  ${FILAMENT_LIBS_3rdPARTY}/zstd/tnt/Debug
  ${FILAMENT_LIBS_3rdPARTY}/civetweb/tnt/Debug
  ${FILAMENT_LIBS_3rdPARTY}/glslang/tnt/glslang/Debug
  ${FILAMENT_LIBS_3rdPARTY}/glslang/tnt/SPIRV/Debug
  ${FILAMENT_LIBS_3rdPARTY}/spirv-cross/tnt/Debug
  ${FILAMENT_LIBS_3rdPARTY}/spirv-tools/source/Debug
  ${FILAMENT_LIBS_3rdPARTY}/smol-v/tnt/Debug

  ${SDL3_ROOT}/build/Debug
)

target_link_libraries(app PUBLIC
    "${FILAMENT_ROOT}/out/filament/Debug/filament.lib"
    "${FILAMENT_ROOT}/out/filament/backend/Debug/backend.lib"
    "${FILAMENT_LIBS_ROOT}/gltfio/Debug/gltfio.lib"
    "${FILAMENT_LIBS_ROOT}/utils/Debug/utils.lib"
    "${FILAMENT_LIBS_ROOT}/math/Debug/math.lib"
    "${SDL3_ROOT}/build/Debug/SDL3.lib"
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
    opengl32
    "${FILAMENT_LIBS_3rdPARTY}/smol-v/tnt/Debug/smol-v.lib"
)

target_sources(app PRIVATE ${RESOURCE_SOURCES_MATDBG} ${FILAMENT_ROOT}/out/libs/matdbg/matdbg_resources.c)

# target_include_directories(app PRIVATE 
#   ${FILAMENT_GENERATED_RESOURCES}
#   ${FILAMENT_GENERATED_RESOURCES_MATERIALS}
#   ${FILAMENT_GENERATED_RESOURCES_DATA}
#   ${FILAMENT_GENERATED_RESOURCES_MATDBG}
# )

# Garante consistência do runtime em todas as targets
set_property(TARGET app PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")

add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/3rd_party/SDL-release-3.2.24/build/Debug/SDL3.dll"
    $<TARGET_FILE_DIR:app>)