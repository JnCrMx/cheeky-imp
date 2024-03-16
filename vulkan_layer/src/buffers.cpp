#include "buffers.hpp"
#include "dispatch.hpp"
#include "layer.hpp"
#include "utils.hpp"
#include "objects.hpp"
#include <vulkan/vulkan_core.h>

using CheekyLayer::logger;
using CheekyLayer::rules::VkHandle;

namespace CheekyLayer {

VkResult device::CreateBuffer(const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
    VkResult ret = dispatch.CreateBuffer(handle, pCreateInfo, pAllocator, pBuffer);

    auto& buffer = buffers[*pBuffer];
    buffer.buffer = *pBuffer;
    buffer.createInfo = *pCreateInfo;

    if(dispatch.SetDebugUtilsObjectNameEXT)
    {
		std::ostringstream names{};
        names << "Buffer " << std::hex << (VkHandle)(*pBuffer);
        std::string name = names.str();

        VkDebugUtilsObjectNameInfoEXT info{};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.objectType = VK_OBJECT_TYPE_BUFFER;
        info.objectHandle = (uint64_t) *pBuffer;
        info.pObjectName = name.c_str();
        dispatch.SetDebugUtilsObjectNameEXT(handle, &info);
    }

    return ret;
}

VkResult device::BindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    buffers[buffer].memory = memory;
    buffers[buffer].memoryOffset = memoryOffset;

    logger->info("BindBufferMemory: buffer={} memory={} offset={:#x}", fmt::ptr(buffer), fmt::ptr(memory), memoryOffset);

    return dispatch.BindBufferMemory(handle, buffer, memory, memoryOffset);
}

VkResult device::MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)
{
    VkResult ret = dispatch.MapMemory(handle, memory, offset, size, flags, ppData);
    logger->info("MapMemory: memory={} offset={:#x} size={:#x} flags={}", fmt::ptr(memory), offset, size, flags);

    if(ret == VK_SUCCESS)
    {
        memoryMappings[memory] = {.pointer = *ppData, .offset = offset, .size = size};
    }
    return ret;
}

void device::UnmapMemory(VkDeviceMemory memory)
{
    dispatch.UnmapMemory(handle, memory);
    logger->info("UnmapMemory: memory={}", fmt::ptr(memory));

    memoryMappings.erase(memory);
}

void device::CmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
{
    auto& src = buffers[srcBuffer];
    auto& dst = buffers[dstBuffer];

    auto offset = src.memoryOffset + pRegions[0].srcOffset;
    auto size = pRegions[0].size;

    void* data;
    VkResult ret = dispatch.MapMemory(handle, src.memory, offset, size, 0, &data);
    if(ret == VK_SUCCESS)
    {
        std::string hash_string = sha256_string((uint8_t*) data, size);
        logger->info("CmdCopyBuffer: src={}@{:x}, dst={}@{:x}, hash={}",
            fmt::ptr(srcBuffer), pRegions[0].srcOffset, fmt::ptr(dstBuffer), pRegions[0].dstOffset, hash_string);

        if(dispatch.SetDebugUtilsObjectNameEXT)
        {
            std::string name = "Buffer "+hash_string;

            VkDebugUtilsObjectNameInfoEXT info{};
            info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            info.objectType = VK_OBJECT_TYPE_BUFFER;
            info.objectHandle = (uint64_t) dstBuffer;
            info.pObjectName = name.c_str();
            dispatch.SetDebugUtilsObjectNameEXT(handle, &info);
        }

        if(inst->config.dump) {
            auto outputPath = inst->config.dump_directory / "buffers" / (hash_string + ".buf");
            std::ofstream out(outputPath, std::ios_base::binary);
            if(out.good())
                out.write((char*)data, (size_t)size);
        }

        dispatch.UnmapMemory(handle, src.memory);
    }
    else
    {
        logger->info("CmdCopyBuffer: src={}@{:x}, dst={}@{:x}",
            fmt::ptr(srcBuffer), pRegions[0].srcOffset, fmt::ptr(dstBuffer), pRegions[0].dstOffset);
        logger->warn("Cannot map memory: {}", fmt::underlying(ret));
    }

    dispatch.CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

}

std::map<VkBuffer, VkBufferCreateInfo> buffers;
std::map<VkBuffer, VkDevice> bufferDevices;
std::map<VkBuffer, VkDeviceMemory> bufferMemories;
std::map<VkBuffer, VkDeviceSize> bufferMemoryOffsets;
std::map<VkDeviceMemory, memory_map_info> memoryMappings;

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
    return CheekyLayer::get_device(device).CreateBuffer(pCreateInfo, pAllocator, pBuffer);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_BindBufferMemory(
        VkDevice                                    device,
        VkBuffer                                    buffer,
        VkDeviceMemory                              memory,
        VkDeviceSize                                memoryOffset)
{
    return CheekyLayer::get_device(device).BindBufferMemory(buffer, memory, memoryOffset);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_MapMemory(
    VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
    VkMemoryMapFlags flags, void** ppData)
{
    return CheekyLayer::get_device(device).MapMemory(memory, offset, size, flags, ppData);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_UnmapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory)
{
    return CheekyLayer::get_device(device).UnmapMemory(memory);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
{
    return CheekyLayer::get_device(commandBuffer).CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
	/*CheekyLayer::active_logger log = *logger << logger::begin;
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
        std::string hash_string = sha256_string((uint8_t*) data, size);

        CheekyLayer::rules::rule_env.hashes[(VkHandle)dstBuffer] = hash_string;
		CheekyLayer::rules::local_context ctx = { .logger = log, .device = device};
		CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::Buffer, (VkHandle)dstBuffer, ctx);

        log << " hash=" << hash_string;

        if(device_dispatch[GetKey(device)].SetDebugUtilsObjectNameEXT)
        {
			std::string name = "Buffer "+hash_string;

            VkDebugUtilsObjectNameInfoEXT info{};
            info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            info.objectType = VK_OBJECT_TYPE_BUFFER;
            info.objectHandle = (uint64_t) dstBuffer;
            info.pObjectName = name.c_str();
            device_dispatch[GetKey(device)].SetDebugUtilsObjectNameEXT(device, &info);
        }

        if(global_config.map<bool>("dump", CheekyLayer::config::to_bool)/* && should_dump(hash_string) * /)
		{
            std::string outputPath = global_config["dumpDirectory"]+"/buffers/"+hash_string+".buf";
            {
                /*{
				    scoped_lock l(global_lock);
                    dumpCache.push_back(hash_string);
                }* /
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

    dispatch.CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);*/
}
