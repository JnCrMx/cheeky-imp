#pragma once

#include <map>
#include "layer.hpp"

extern std::map<VkBuffer, VkBufferCreateInfo> buffers;
extern std::map<VkBuffer, VkDevice> bufferDevices;
extern std::map<VkBuffer, VkDeviceMemory> bufferMemories;
extern std::map<VkBuffer, VkDeviceSize> bufferMemoryOffsets;
