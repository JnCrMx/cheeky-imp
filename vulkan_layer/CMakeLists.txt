cmake_minimum_required (VERSION 3.4)
project(vulkan_layer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED FATAL_ERROR)
find_package(OpenSSL REQUIRED)

file(GLOB_RECURSE sources src/**.cpp)

add_library(cheeky_layer SHARED ${sources})

target_include_directories(cheeky_layer PUBLIC include/)
target_include_directories(cheeky_layer PUBLIC ${Vulkan_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
  
target_link_libraries(cheeky_layer PUBLIC ${Vulkan_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})

option(CHEEKY_LAYER_SPIRV_CROSS "Decompile shaders with SPIRV-Cross. (very experimental)"   OFF)
option(CHEEKY_LAYER_GLSLANG     "Compile shaders with glslang. (experimental)"              ON)
option(CHEEKY_LAYER_IMAGE_TOOLS "Use image_tools to load and convert png images."           ON)

if(CHEEKY_LAYER_SPIRV_CROSS)
    set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS OFF CACHE BOOL "")
    set(SPIRV_CROSS_SHARED OFF CACHE BOOL "")
    set(SPIRV_CROSS_STATIC ON CACHE BOOL "")
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "")
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "")
    set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "")
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "")
    set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "")
    set(SPIRV_CROSS_ENABLE_CPP ON CACHE BOOL "")
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "")
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "")
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "")
    set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "")
    set(SPIRV_CROSS_FORCE_PIC ON CACHE BOOL "")

    add_subdirectory(external/SPIRV-Cross)

    target_link_libraries(cheeky_layer PRIVATE spirv-cross-core spirv-cross-cpp)
    target_include_directories(cheeky_layer PRIVATE external/SPIRV-Cross)

    target_compile_definitions(cheeky_layer PUBLIC USE_SPIRV)
endif()

if(CHEEKY_LAYER_GLSLANG)
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Skip installation")
    set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "Builds glslangValidator and spirv-remap")
    set(ENABLE_CTEST OFF CACHE BOOL "Enables testing")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Shared Libraries")
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Enables building of SPVRemapper")
    set(ENABLE_HLSL OFF CACHE BOOL "Enables HLSL input support")

    add_subdirectory(external/glslang)

    target_link_libraries(cheeky_layer PUBLIC glslang SPIRV glslang-default-resource-limits)
    target_include_directories(cheeky_layer PUBLIC external/glslang/glslang/Public external/glslang/SPIRV external/glslang/StandAlone)

    target_compile_definitions(cheeky_layer PUBLIC USE_GLSLANG)
endif()

if(CHEEKY_LAYER_IMAGE_TOOLS)
    set(IMAGE_TOOLS_CLI OFF CACHE BOOL "Build 'pack' and 'unpack' executables")
    set(IMAGE_TOOLS_PIC ON CACHE BOOL "Use -fPIC for compiling the library")

    add_subdirectory(external/image_tools)

    target_link_libraries(cheeky_layer PUBLIC image_tools)
	target_include_directories(cheeky_layer PRIVATE external/stb)

    target_compile_definitions(cheeky_layer PUBLIC USE_IMAGE_TOOLS)
endif()
