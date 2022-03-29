#include <iostream>

#include <ShaderLang.h>
#include <ResourceLimits.h>
#include <GlslangToSpv.h>

#include "shaders.hpp"

int main()
{
	std::string glsl = R"EOF(
		#version 450

		layout(push_constant) uniform pushConstants {
			int indexCount;
			vec4 test;
		} u_pushConstants;

		layout(set = 0, binding = 0, std140) uniform datahook2_t
		{
			vec4 camera_position;
			vec4 character_position;
		} datahook2;

		layout(set = 1, binding = 0, std140) buffer datahook_t
		{
			vec4 camera_position;
			vec4 character_position;
		} datahook;

		void main()
		{
			datahook.camera_position = vec4(u_pushConstants.indexCount, 1.0, 2.0, 3.0) - u_pushConstants.test + datahook2.camera_position;
		}
	)EOF";

	std::vector<unsigned int> bin;
	ReflectionInfo info;
	
	auto [sucess, error ] = compileShader(EShLangVertex, glsl, bin, info);

	for(auto& t : info.uniformBlockReflection)
	{
		std::cout << t.name << ": [" << t.index << "] " << std::boolalpha << t.qualifier.isPushConstant() << " " << t.offset << " " << t.size << std::endl;
	}
	for(auto& t : info.uniformVariableReflection)
	{
		std::cout << t.name << ": [" << t.index << "] " << t.offset << " " << t.glDefineType << std::endl;
	}

	std::cout << std::boolalpha << sucess << ": " << error << std::endl;

	return 0;
}
