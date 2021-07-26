#include "utils.hpp"
#include "layer.hpp"

#include <algorithm>
#include <cstdio>
#include <openssl/sha.h>

void sha256_string(const uint8_t *buf, size_t len, char outputBuffer[65])
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, buf, len);
	SHA256_Final(hash, &sha256);
	int i = 0;
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
	}
	outputBuffer[64] = 0;
}

bool has_override(std::string hash)
{
	return std::find(overrideCache.begin(), overrideCache.end(), hash) != overrideCache.end();
}

void replace(std::string& string, const std::string search, const std::string replacement)
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
