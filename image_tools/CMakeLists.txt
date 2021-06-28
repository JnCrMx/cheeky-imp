cmake_minimum_required(VERSION 3.10)

project(image_tools) 

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glm REQUIRED)

file(GLOB_RECURSE sources_lib src/lib/*.cpp)
file(GLOB_RECURSE sources_pack src/pack.cpp)
file(GLOB_RECURSE sources_unpack src/unpack.cpp)

option(IMAGE_TOOLS_CLI "Build 'pack' and 'unpack' executables"	ON)
option(IMAGE_TOOLS_PIC "Create position independent code"   	OFF)

add_library(image_tools ${sources_lib})
target_include_directories(image_tools PUBLIC include/)
target_include_directories(image_tools PRIVATE ${GLM_INCLUDE_DIRS})
if(IMAGE_TOOLS_PIC)
	set_property(TARGET image_tools PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if(IMAGE_TOOLS_CLI)
	add_executable(pack ${sources_pack})
	target_include_directories(pack PRIVATE external/stb)
	target_link_libraries(pack PRIVATE image_tools)

	add_executable(unpack ${sources_unpack})
	target_include_directories(unpack PRIVATE external/stb)
	target_link_libraries(unpack PRIVATE image_tools)
endif()
