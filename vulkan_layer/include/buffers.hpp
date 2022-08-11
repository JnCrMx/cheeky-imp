#pragma once

#include <map>
#include "layer.hpp"

extern std::map<VkBuffer, VkBufferCreateInfo> buffers;
extern std::map<VkBuffer, VkDevice> bufferDevices;
extern std::map<VkBuffer, VkDeviceMemory> bufferMemories;
extern std::map<VkBuffer, VkDeviceSize> bufferMemoryOffsets;

struct memory_map_info
{
	void* pointer;
	VkDeviceSize offset;
	VkDeviceSize size;
};
extern std::map<VkDeviceMemory, memory_map_info> memoryMappings;
