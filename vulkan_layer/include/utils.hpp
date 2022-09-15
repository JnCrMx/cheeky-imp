#pragma once

#include "buffers.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <map>
#include <vulkan/vulkan.h>

void sha256_string(const uint8_t *buf, size_t len, char outputBuffer[65]);
bool should_dump(std::string hash);
bool has_override(std::string hash);

void replace(std::string& string, const std::string search, const std::string replacement);

uint32_t findMemoryType(VkPhysicalDeviceMemoryProperties memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties);

void memoryAccess(VkDevice device, VkDeviceMemory memory, std::function<void(void*, VkDeviceSize)> function, VkDeviceSize offset = 0);
void memoryAccess(VkDevice device, VkBuffer buffer, std::function<void(void*, VkDeviceSize)> function, VkDeviceSize offset = 0);
