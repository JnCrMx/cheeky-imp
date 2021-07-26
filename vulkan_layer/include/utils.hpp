#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vulkan/vulkan.h>

void sha256_string(const uint8_t *buf, size_t len, char outputBuffer[65]);
bool has_override(std::string hash);

void replace(std::string& string, const std::string search, const std::string replacement);

uint32_t findMemoryType(VkPhysicalDeviceMemoryProperties memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties);
