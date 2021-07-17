#pragma once

#include <string>
#include <map>
#include <vulkan/vulkan.h>

namespace CheekyLayer { namespace reflection {
	struct VkReflectInfo
	{
		std::string name;
		std::string type;
		bool pointer;
		int offset;
	};

	typedef std::map<std::string, std::map<std::string, VkReflectInfo>> reflection_map;
	extern reflection_map struct_reflection_map;
}}