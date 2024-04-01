#include "utils.hpp"

#include <cstdio>
#include <openssl/sha.h>
#include <stdexcept>

std::string sha256_string(const std::span<const unsigned char> data)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256(data.data(), data.size(), hash);

	char outputBuffer[65];
	int i = 0;
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
	}
	outputBuffer[64] = 0;

	return std::string(outputBuffer);
}
std::string sha256_string(const unsigned char* data, std::size_t len) {
	return sha256_string(std::span(data, len));
}

void replace(std::string& string, const std::string& search, const std::string& replacement)
{
	if(search.empty())
		return;
	std::string::size_type pos = 0;
	while((pos = string.find(search, pos)) != std::string::npos)
	{
		string.replace(pos, search.length(), replacement);
		pos += replacement.length();
	}
}

uint32_t findMemoryType(VkPhysicalDeviceMemoryProperties memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if((typeFilter & (1 << i)) &&
				(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}
