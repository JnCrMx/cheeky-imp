cmake_minimum_required(VERSION 3.10)

project(texture_renderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED COMPONENTS glslang)
find_package(glm REQUIRED)

include(FetchContent)
FetchContent_Declare(cxxopts GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
                             GIT_TAG        v3.1.1)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(cxxopts json)

set(SOURCES main.cpp)

add_executable(texture_renderer ${SOURCES})
target_link_libraries(texture_renderer PUBLIC 
    Vulkan::Vulkan Vulkan::glslang Vulkan::Headers glm
    cxxopts::cxxopts nlohmann_json::nlohmann_json
    /usr/lib/libglslang-default-resource-limits.a
)
target_include_directories(texture_renderer PRIVATE external/stb)
