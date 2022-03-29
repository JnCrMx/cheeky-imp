#pragma once

#include <map>
#include "rules/execution_env.hpp"

using CheekyLayer::rules::VkHandle;
extern std::map<VkHandle, VkHandle> customShaderHandles;

#ifdef USE_GLSLANG
#include <ShaderLang.h>
#include <ResourceLimits.h>
#include <GlslangToSpv.h>
struct ReflectionElement
{
	std::string name;
    int offset;
    int glDefineType;
    int size;                   // data size in bytes for a block, array size for a (non-block) object that's an array
    int index;
    int counterIndex;
    int numMembers;
    int arrayStride;            // stride of an array variable
    int topLevelArraySize;      // size of the top-level variable in a storage buffer member
    int topLevelArrayStride;    // stride of the top-level variable in a storage buffer member
    EShLanguageMask stages;

	glslang::TQualifier qualifier;

	ReflectionElement() = default;

	ReflectionElement(const glslang::TObjectReflection& reflection) :
		name(reflection.name), offset(reflection.offset), glDefineType(reflection.glDefineType), size(reflection.size),
		index(reflection.index), counterIndex(reflection.counterIndex), numMembers(reflection.numMembers),
		arrayStride(reflection.arrayStride), topLevelArraySize(reflection.topLevelArraySize),
		topLevelArrayStride(reflection.topLevelArrayStride), stages(reflection.stages)
	{

	}

	ReflectionElement(const glslang::TObjectReflection& reflection, const glslang::TType* t) : ReflectionElement(reflection)
	{
		qualifier = t->getQualifier();
	}
};

struct ReflectionInfo
{
	std::vector<ReflectionElement> bufferVariableReflection;
	std::vector<ReflectionElement> bufferBlockReflection;
	std::vector<ReflectionElement> uniformVariableReflection;
	std::vector<ReflectionElement> uniformBlockReflection;
};
extern std::map<VkShaderModule, ReflectionInfo> shaderReflections;

extern std::tuple<bool, std::string> compileShader(EShLanguage stage, std::string glslCode, std::vector<unsigned int>& shaderCode, ReflectionInfo& reflectionInfo);
#endif
