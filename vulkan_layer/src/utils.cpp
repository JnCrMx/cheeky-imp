#include "utils.hpp"
#include "layer.hpp"
#include "dispatch.hpp"

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

void memoryAccess(VkDevice device, VkDeviceMemory memory, std::function<void(void*, VkDeviceSize)> function, VkDeviceSize offset)
{
	if(memoryMappings.contains(memory))
	{
		const auto& mapping = memoryMappings[memory];
		auto ptrOffset = offset - mapping.offset;
		if(mapping.offset > offset || ptrOffset > mapping.size)
			throw std::runtime_error("memory is mapped, but requested region cannot be accessed");
		function((uint8_t*)mapping.pointer + ptrOffset, mapping.size - ptrOffset);

		VkMappedMemoryRange range{};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = memory;
		range.offset = mapping.offset;
		range.size = mapping.size;
		device_dispatch[GetKey(device)].FlushMappedMemoryRanges(device, 1, &range);
	}
	else
	{
		void* ptr;
		device_dispatch[GetKey(device)].MapMemory(device, memory, offset, VK_WHOLE_SIZE, 0, &ptr);
		function(ptr, VK_WHOLE_SIZE);

		VkMappedMemoryRange range{};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = memory;
		range.offset = offset;
		range.size = VK_WHOLE_SIZE;
		device_dispatch[GetKey(device)].FlushMappedMemoryRanges(device, 1, &range);

		device_dispatch[GetKey(device)].UnmapMemory(device, memory);
	}
}

void memoryAccess(VkDevice device, VkBuffer buffer, std::function<void(void*, VkDeviceSize)> function, VkDeviceSize offset)
{
	VkDeviceMemory memory = bufferMemories.at(buffer);
	VkDeviceSize memoryOffset = bufferMemoryOffsets.at(buffer);

	memoryAccess(device, memory, function, memoryOffset + offset);
}
