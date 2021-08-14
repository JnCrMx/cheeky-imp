cmake_minimum_required (VERSION 3.4)
include(CMakeDependentOption)

project(vulkan_layer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED FATAL_ERROR)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

file(GLOB_RECURSE sources src/**.cpp external/Vulkan-ValidationLayers/layers/vk_format_utils.cpp)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vkreflectionmap.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate_reflection.py
    COMMAND PYTHONPATH=/usr/share/vulkan/registry/ python ${CMAKE_CURRENT_SOURCE_DIR}/generate_reflection.py > ${CMAKE_CURRENT_BINARY_DIR}/vkreflectionmap.cpp)
list(APPEND sources ${CMAKE_CURRENT_BINARY_DIR}/vkreflectionmap.cpp)

add_library(cheeky_layer SHARED ${sources})

target_include_directories(cheeky_layer PUBLIC include/)
target_include_directories(cheeky_layer PUBLIC external/Vulkan-ValidationLayers/layers/)
target_include_directories(cheeky_layer PUBLIC ${Vulkan_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
  
target_link_libraries(cheeky_layer PUBLIC ${Vulkan_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries(cheeky_layer PRIVATE Threads::Threads)

option(CHEEKY_LAYER_SPIRV_CROSS "Decompile shaders with SPIRV-Cross. (very experimental)"   OFF)
option(CHEEKY_LAYER_GLSLANG     "Compile shaders with glslang. (experimental)"              ON)
option(CHEEKY_LAYER_IMAGE_TOOLS "Use image_tools to load and convert png images."           ON)
cmake_dependent_option(CHEEKY_LAYER_EXPORT_PNG "Export images as png using image_tools." ON "CHEEKY_LAYER_IMAGE_TOOLS" OFF)

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
    set(IMAGE_TOOLS_WITH_VULKAN	ON CACHE BOOL "Integrate with VkFormat")

    add_subdirectory(external/image_tools)
    target_compile_options(image_tools PRIVATE "-O3")

    target_link_libraries(cheeky_layer PUBLIC image_tools)
	target_include_directories(cheeky_layer PRIVATE external/stb)

    target_compile_definitions(cheeky_layer PUBLIC USE_IMAGE_TOOLS)
    if(CHEEKY_LAYER_EXPORT_PNG)
        target_compile_definitions(cheeky_layer PUBLIC EXPORT_PNG)
    endif()
endif()

find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    file(DOWNLOAD https://upload.cppreference.com/mwiki/images/f/f8/cppreference-doxygen-web.tag.xml ${CMAKE_CURRENT_BINARY_DIR}/cppreference-doxygen-web.tag.xml)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target(doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)

enable_testing()
add_subdirectory(test)
