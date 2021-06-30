cmake_minimum_required(VERSION 3.10)

project(mesh_buffer_tools)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.40 COMPONENTS system REQUIRED)
find_package(glm REQUIRED)
find_package(libxml2 REQUIRED)

file(GLOB_RECURSE sources_common src/common/*.cpp)
file(GLOB_RECURSE sources_extract src/extract/*.cpp)
file(GLOB_RECURSE sources_pose src/pose/*.cpp)
file(GLOB_RECURSE sources_patch src/patch/*.cpp)

option(PATCH_APPROX_UNIQUE "Approximate unique vertices (reduces vertex count)" ON)

add_executable(extract ${sources_common} ${sources_extract})
add_executable(pose ${sources_common} ${sources_pose})
add_executable(patch ${sources_common} ${sources_patch})

if(PATCH_APPROX_UNIQUE)
	target_compile_definitions(patch PUBLIC "APPROX_UNIQUE")
endif()

include_directories(include/)
include_directories(${Boost_INCLUDE_DIR})
include_directories(${GLM_INCLUDE_DIRS})

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(external/json)

target_link_libraries(extract PUBLIC ${Boost_SYSTEM_LIBRARY})
target_link_libraries(extract PUBLIC nlohmann_json::nlohmann_json)

target_link_libraries(pose PUBLIC ${Boost_SYSTEM_LIBRARY})
target_link_libraries(pose PUBLIC nlohmann_json::nlohmann_json)

target_link_libraries(patch PUBLIC ${Boost_SYSTEM_LIBRARY})
target_link_libraries(patch PUBLIC nlohmann_json::nlohmann_json)
