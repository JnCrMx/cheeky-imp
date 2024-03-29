#include "constants.hpp"
#include "git.h"

namespace CheekyLayer {namespace Contants
{
	const std::string LAYER_NAME = "VK_LAYER_CHEEKYIMP_CheekyLayer";
	const std::string LAYER_DESCRIPTION = "Vulkan layer for cheeky-imp - https://github.com/JnCrMx/cheeky-imp";
	const std::string CONFIG_ENV = "CHEEKY_LAYER_CONFIG";
	const std::string GIT_VERSION = std::string{git::Describe()}+(git::AnyUncommittedChanges()?"-dirty":"");
}}
