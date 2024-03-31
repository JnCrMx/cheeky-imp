#pragma once

#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <vulkan/vulkan.h>

std::string sha256_string(std::span<const unsigned char> data);
std::string sha256_string(const unsigned char *buf, std::size_t len);

void replace(std::string& string, const std::string& search, const std::string& replacement);

uint32_t findMemoryType(VkPhysicalDeviceMemoryProperties memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties);
