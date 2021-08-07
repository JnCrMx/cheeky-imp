#pragma once

#include <map>
#include "rules/execution_env.hpp"

using CheekyLayer::VkHandle;
extern std::map<VkHandle, VkHandle> customShaderHandles;

#ifdef USE_GLSLANG
#include <ShaderLang.h>
extern std::tuple<bool, std::string> compileShader(EShLanguage stage, std::string glslCode, std::vector<unsigned int>& shaderCode);
#endif
