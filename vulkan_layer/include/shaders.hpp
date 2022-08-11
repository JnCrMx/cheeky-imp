#pragma once

#include <map>
#include "rules/execution_env.hpp"
#include "spirv_reflect.h"

using CheekyLayer::rules::VkHandle;
extern std::map<VkHandle, VkHandle> customShaderHandles;

extern std::map<VkHandle, spv_reflect::ShaderModule> shaderReflections;

#ifdef USE_GLSLANG
#include <ShaderLang.h>
extern std::tuple<bool, std::string> compileShader(EShLanguage stage, std::string glslCode, std::vector<unsigned int>& shaderCode);
#endif
