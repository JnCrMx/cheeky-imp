#include "buffers.hpp"
#include "dispatch.hpp"
#include "layer.hpp"
#include "utils.hpp"
#include <vulkan/vulkan_core.h>

#include <filesystem>

using CheekyLayer::logger;
using CheekyLayer::rules::VkHandle;

std::map<VkBuffer, VkBufferCreateInfo> buffers;
std::map<VkBuffer, VkDevice> bufferDevices;
std::map<VkBuffer, VkDeviceMemory> bufferMemories;
std::map<VkBuffer, VkDeviceSize> bufferMemoryOffsets;

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
    VkResult ret = device_dispatch[GetKey(device)].CreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
    buffers[*pBuffer] = *pCreateInfo;
    bufferDevices[*pBuffer] = device;

    return ret;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_BindBufferMemory(
        VkDevice                                    device,
        VkBuffer                                    buffer,
        VkDeviceMemory                              memory,
        VkDeviceSize                                memoryOffset)
{
    try
    {
        bufferMemories[buffer]=memory;
        bufferMemoryOffsets[buffer]=memoryOffset;

        *logger << logger::begin << "BindBufferMemory: buffer=" << buffer << " memory=" << memory << " offset=" << std::hex << memoryOffset << logger::end;

        return device_dispatch[GetKey(device)].BindBufferMemory(device, buffer, memory, memoryOffset);
    }
    catch(const std::exception& ex)
    {
        *logger << logger::begin << logger::error << "BindBufferMemory: " << ex.what() << logger::end;
    }
    return VK_ERROR_UNKNOWN;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_MapMemory(
    VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
    VkMemoryMapFlags flags, void** ppData)
{
    *logger << logger::begin << "MapMemory: memory=" << memory << " offset=" << offset << " size=" << size << " flags=" << flags << logger::end;
    
    return device_dispatch[GetKey(device)].MapMemory(device, memory, offset, size, flags, ppData);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
{
	CheekyLayer::active_logger log = *logger << logger::begin;
    log << "CmdCopyBuffer: src=" << srcBuffer << " @ " << std::hex << pRegions[0].srcOffset << std::dec
            << " dst=" << dstBuffer << " @ " << std::hex << pRegions[0].dstOffset << std::dec;
    log.flush();

    VkDevice device = bufferDevices[srcBuffer];
    VkDeviceMemory memory = bufferMemories[srcBuffer];
    VkLayerDispatchTable dispatch = device_dispatch[GetKey(device)];

    auto offset = bufferMemoryOffsets[srcBuffer] + pRegions[0].srcOffset;
    auto size = pRegions[0].size;

    void* data;
    VkResult ret = dispatch.MapMemory(device, memory, offset, size, 0, &data);
    if(ret == VK_SUCCESS)
    {
        char hash[65];
        sha256_string((uint8_t*)data, (size_t)size, hash);
        std::string hash_string(hash);

        CheekyLayer::rules::rule_env.hashes[(VkHandle)dstBuffer] = hash_string;
		CheekyLayer::rules::local_context ctx = { .logger = log, .device = device};
		CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::Buffer, (VkHandle)dstBuffer, ctx);

        log << " hash=" << hash;
        if(global_config.map<bool>("dump", CheekyLayer::config::to_bool))
		{
            std::string outputPath = global_config["dumpDirectory"]+"/buffers/"+hash_string+".buf";
            if(!std::filesystem::exists(outputPath))
            {
                std::ofstream out(outputPath, std::ios_base::binary);
                if(out.good())
                    out.write((char*)data, (size_t)size);
            }
        }

        if(global_config.map<bool>("override", CheekyLayer::config::to_bool) && has_override(hash_string))
		{
            std::ifstream in(global_config["overrideDirectory"]+"/buffers/"+hash_string+".buf", std::ios_base::binary);
            if(in.good())
            {
                log << " Found buffer override!";
                memset(data, 0, size);
                in.read((char*)data, size);
            }
        }

        dispatch.UnmapMemory(device, memory);
    }
    else
    {
        log << logger::error << " Cannot map memory " << ret;
    }
   	log << logger::end;

    dispatch.CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}
