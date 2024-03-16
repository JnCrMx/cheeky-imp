#pragma once

#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <functional>
#include <vulkan/vulkan.h>

std::string sha256_string(std::span<const unsigned char> data);
std::string sha256_string(const unsigned char *buf, std::size_t len);
bool should_dump(const std::string& hash);
bool has_override(const std::string& hash);

void replace(std::string& string, const std::string& search, const std::string& replacement);

uint32_t findMemoryType(VkPhysicalDeviceMemoryProperties memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties);

void memoryAccess(VkDevice device, VkDeviceMemory memory, std::function<void(void*, VkDeviceSize)> function, VkDeviceSize offset = 0);
void memoryAccess(VkDevice device, VkBuffer buffer, std::function<void(void*, VkDeviceSize)> function, VkDeviceSize offset = 0);
