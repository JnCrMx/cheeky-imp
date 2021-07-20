#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
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

	typedef std::unordered_map<std::string, VkReflectInfo> inner_reflection_map;
	typedef std::unordered_map<std::string, VkEnumEntry> inner_enum_map;

	typedef std::unordered_map<std::string, inner_reflection_map> reflection_map;
	typedef std::unordered_map<std::string, inner_enum_map> enum_map;

	extern reflection_map struct_reflection_map;
	extern enum_map enum_reflection_map;
}}