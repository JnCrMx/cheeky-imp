cmake_minimum_required(VERSION 3.10)

project(texture_renderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glslang REQUIRED)

include(FetchContent)
FetchContent_Declare(cxxopts GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
                             GIT_TAG        v3.1.1)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(cxxopts json)

set(SOURCES main.cpp)

add_executable(texture_renderer ${SOURCES})
target_link_libraries(texture_renderer PUBLIC
    Vulkan::Vulkan Vulkan::Headers
    cxxopts::cxxopts nlohmann_json::nlohmann_json
    glslang::SPIRV glslang::glslang-default-resource-limits
)
target_include_directories(texture_renderer PRIVATE external/stb)
