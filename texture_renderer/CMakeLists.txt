cmake_minimum_required(VERSION 3.28)

project(texture_renderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)

include(FetchContent)
FetchContent_Declare(cxxopts GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
                             GIT_TAG        v3.3.1)
FetchContent_Declare(glslang GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
                             GIT_TAG        15.3.0)
FetchContent_Declare(glaze   GIT_REPOSITORY https://github.com/stephenberry/glaze.git
                             GIT_TAG        v5.1.2)

set(ENABLE_OPT OFF)
FetchContent_MakeAvailable(cxxopts glslang glaze)

set(SOURCES main.cpp)

add_executable(texture_renderer ${SOURCES})
target_link_libraries(texture_renderer PUBLIC
    Vulkan::Vulkan Vulkan::Headers
    cxxopts::cxxopts glaze::glaze
    SPIRV glslang-default-resource-limits
)
target_include_directories(texture_renderer PRIVATE external/stb)
