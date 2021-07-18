#pragma once

#include <cstdint>
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

	struct VkEnumEntry
	{
		std::string name;
		uint32_t value;
	};

	typedef std::map<std::string, std::map<std::string, VkReflectInfo>> reflection_map;
	typedef std::map<std::string, std::map<std::string, VkEnumEntry>> enum_map;

	extern reflection_map struct_reflection_map;
	extern enum_map enum_reflection_map;
}}