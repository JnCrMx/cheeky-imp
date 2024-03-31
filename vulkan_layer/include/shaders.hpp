#pragma once

#ifdef USE_GLSLANG
#include <ShaderLang.h>
extern std::tuple<bool, std::string> compileShader(EShLanguage stage, std::string glslCode, std::vector<unsigned int>& shaderCode);
#endif
