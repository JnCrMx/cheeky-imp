cmake_minimum_required(VERSION 3.10)

project(bone_analyzer) 

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glm REQUIRED)

file(GLOB_RECURSE sources src/multi_joint_method.cpp)

add_executable(bone_analyzer ${sources})

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(external/json)

target_link_libraries(bone_analyzer PUBLIC nlohmann_json::nlohmann_json)

target_include_directories(bone_analyzer PUBLIC ${GLM_INCLUDE_DIRS})
