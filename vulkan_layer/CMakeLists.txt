cmake_minimum_required (VERSION 3.5)
include(CMakeDependentOption)
include(FetchContent)

project(vulkan_layer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(Backward REQUIRED)

set(sources
    "src/buffers.cpp"
    "src/config.cpp"
    "src/constants.cpp"
    "src/descriptors.cpp"
    "src/dispatch.cpp"
    "src/draw.cpp"
    "src/images.cpp"
    "src/layer.cpp"
    "src/shaders.cpp"
    "src/utils.cpp"
    "src/objects.cpp"
    "src/reflection/reflectionparser.cpp"
    "src/rules/actions.cpp"
    "src/rules/conditions.cpp"
    "src/rules/data.cpp"
    "src/rules/ipc.cpp"
    "src/rules/rules.cpp"
    "src/rules/data/convert.cpp"
    "src/rules/data/functions.cpp"
    "src/rules/data/map.cpp"
    "src/rules/data/math.cpp"
    "src/rules/data/pack.cpp"
    "src/rules/data/reduce.cpp"
    "src/rules/data/variables.cpp"
    "src/rules/data/vulkan.cpp"

    "external/Vulkan-ValidationLayers/layers/vk_format_utils.cpp"
)
set(includes
    "include/config.hpp"
    "include/constants.hpp"
    "include/dispatch.hpp"
    "include/layer.hpp"
    "include/shaders.hpp"
    "include/utils.hpp"
    "include/objects.hpp"
    "include/reflection/custom_structs.hpp"
    "include/reflection/reflectionparser.hpp"
    "include/reflection/vkreflection.hpp"
    "include/rules/actions.hpp"
    "include/rules/conditions.hpp"
    "include/rules/data.hpp"
    "include/rules/execution_env.hpp"
    "include/rules/ipc.hpp"
    "include/rules/reader.hpp"
    "include/rules/rules.hpp"
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vkreflectionmap.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate_reflection.py
    COMMAND PYTHONPATH=/usr/share/vulkan/registry/ python3 ${CMAKE_CURRENT_SOURCE_DIR}/generate_reflection.py > ${CMAKE_CURRENT_BINARY_DIR}/vkreflectionmap.cpp)
list(APPEND sources ${CMAKE_CURRENT_BINARY_DIR}/vkreflectionmap.cpp)

add_library(cheeky_layer SHARED ${sources} ${includes})
set_target_properties(cheeky_layer PROPERTIES PUBLIC_HEADER "${includes}")

target_include_directories(cheeky_layer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:include/cheeky_layer>
)
target_include_directories(cheeky_layer PRIVATE external/Vulkan-ValidationLayers/layers/)
target_include_directories(cheeky_layer PRIVATE external/exprtk)

target_link_libraries(cheeky_layer PUBLIC Vulkan::Vulkan)
target_link_libraries(cheeky_layer PUBLIC spdlog::spdlog)
target_link_libraries(cheeky_layer PUBLIC Backward::Backward)
target_link_libraries(cheeky_layer PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(cheeky_layer PRIVATE Threads::Threads)

option(CHEEKY_LAYER_SPIRV_CROSS "Decompile shaders with SPIRV-Cross. (very experimental)"   OFF)
option(CHEEKY_LAYER_GLSLANG     "Compile shaders with glslang. (experimental)"              ON)
option(CHEEKY_LAYER_IMAGE_TOOLS "Use image_tools to load and convert png images."           ON)
option(CHEEKY_LAYER_DOCS        "Generate documentation using Doxygen"                      ON)
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
    find_package(glslang REQUIRED)
    target_link_libraries(cheeky_layer PUBLIC glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV)
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

add_library(spirv-reflect STATIC external/SPIRV-Reflect/spirv_reflect.c external/SPIRV-Reflect/spirv_reflect.h)
target_include_directories(spirv-reflect PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/SPIRV-Reflect/>
    $<INSTALL_INTERFACE:include/SPIRV-Reflect>)
set_target_properties(spirv-reflect PROPERTIES PUBLIC_HEADER external/SPIRV-Reflect/spirv_reflect.h)
target_link_libraries(cheeky_layer PUBLIC spirv-reflect)

if(CHEEKY_LAYER_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
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
    else(DOXYGEN_FOUND)
        message(FATAL "Doxygen need to be installed to generate the doxygen documentation")
    endif(DOXYGEN_FOUND)
endif(CHEEKY_LAYER_DOCS)

# enable_testing()
# add_subdirectory(test)

FetchContent_Declare(cmake_git_version_tracking
  GIT_REPOSITORY https://github.com/andrew-hardin/cmake-git-version-tracking.git
  GIT_TAG 904dbda1336ba4b9a1415a68d5f203f576b696bb
)
FetchContent_MakeAvailable(cmake_git_version_tracking)
target_link_libraries(cheeky_layer PRIVATE cmake_git_version_tracking)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    CheekyLayerConfigVersion.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(TARGETS spirv-reflect
    EXPORT CheekyLayerTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/SPIRV-Reflect)


install(TARGETS image_tools
    EXPORT CheekyLayerTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/image_tools)

install(TARGETS cheeky_layer
    EXPORT CheekyLayerTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/cheeky_layer
)
install(EXPORT CheekyLayerTargets
    FILE CheekyLayerTargets.cmake
    NAMESPACE CheekyImp::
    DESTINATION lib/cmake/CheekyImp
)

configure_file(CheekyLayerConfig.cmake.in CheekyLayerConfig.cmake @ONLY)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CheekyLayerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CheekyLayerConfigVersion.cmake"
    DESTINATION lib/cmake/CheekyImp)

export(TARGETS cheeky_layer spirv-reflect image_tools NAMESPACE CheekyImp:: FILE CheekyLayerTargets.cmake)

#target_link_libraries(cheeky_layer PUBLIC -fsanitize=address)
#target_compile_options(cheeky_layer PUBLIC -fsanitize=address)
