cmake_minimum_required(VERSION 3.10)

project(image_tools) 

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glm REQUIRED)

file(GLOB_RECURSE sources_common src/common/*.cpp external/lodepng/lodepng.cpp)
file(GLOB_RECURSE sources_pack src/pack/*.cpp)
file(GLOB_RECURSE sources_unpack src/unpack/*.cpp)

add_executable(pack ${sources_common} ${sources_pack})
add_executable(unpack ${sources_common} ${sources_unpack})

include_directories(include/)
include_directories(external/stb)
include_directories(${GLM_INCLUDE_DIRS})
