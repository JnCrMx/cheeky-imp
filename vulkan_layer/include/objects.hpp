#pragma once

#include "config.hpp"
#include "dispatch.hpp"
#include "rules/rules.hpp"
#include <memory>
#include <spdlog/logger.h>
#include <unordered_map>
#include <unordered_set>
#include <vulkan/vulkan_core.h>
#include <vulkan/generated/vk_layer_dispatch_table.h>

namespace CheekyLayer {

struct instance;

struct device {
    device() = default;
    device(instance* inst, PFN_vkGetDeviceProcAddr gdpa, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, VkDevice *pDevice);

    instance* inst;
    std::shared_ptr<spdlog::logger> logger;

    VkDevice handle;
    PFN_vkGetDeviceProcAddr gdpa;
    VkLayerDispatchTable dispatch;

    bool has_debug = false;

    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties memProperties;
    std::vector<VkQueueFamilyProperties> queueFamilies;

    VkQueue transferQueue = VK_NULL_HANDLE;
    VkCommandPool transferPool;
    VkCommandBuffer transferBuffer;

    struct buffer {
        VkBuffer buffer;
        VkBufferCreateInfo createInfo;
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
    };
    struct memory_map_info {
        void* pointer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };
    std::map<VkBuffer, buffer> buffers;
    std::map<VkDeviceMemory, memory_map_info> memoryMappings;

    struct image {
        VkImage image;
        VkImageCreateInfo createInfo;
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        VkImageView view;
    };
    std::map<VkImage, image> images;

    void GetDeviceQueue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue);

    // images.cpp
    VkResult CreateImage(const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*);
    VkResult BindImageMemory(VkImage, VkDeviceMemory, VkDeviceSize);
    VkResult CreateImageView(const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView*);
    void CmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*);

    // buffers.cpp
    VkResult CreateBuffer(const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*);
    VkResult BindBufferMemory(VkBuffer, VkDeviceMemory, VkDeviceSize);
    VkResult MapMemory(VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**);
    void UnmapMemory(VkDeviceMemory);
    void CmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*);

    // draw.cpp
    VkResult CreateSwapchainKHR(const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*);
};

struct instance {
    static unsigned int instance_id;

    instance() = default;
    instance(const VkInstanceCreateInfo *pCreateInfo, VkInstance* pInstance);

    instance& operator=(const instance&) = delete; // no copy
    instance(const instance&) = delete; // no copy

    instance& operator=(instance&&);

    unsigned int id;
    VkInstance handle;
    VkLayerInstanceDispatchTable dispatch;

    CheekyLayer::config config;
    bool enabled = false;
    bool hook_draw_calls = false;

    std::vector<std::unique_ptr<CheekyLayer::rules::rule>> rules;
    std::map<CheekyLayer::rules::selector_type, bool> has_rules;

    std::unordered_set<std::string> overrideCache;
    std::unordered_set<std::string> dumpCache;

    std::mutex lock;
    std::shared_ptr<spdlog::logger> logger;

    std::unordered_map<VkDevice, device*> devices;

    VkResult CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);
};

inline std::unordered_map<void*, std::unique_ptr<instance>> instances;
inline std::unordered_map<void*, std::unique_ptr<device>> devices;

inline instance& get_instance(VkInstance instance) {
    return *instances.at(GetKey(instance));
}
inline instance& get_instance(VkPhysicalDevice physicalDevice) {
    return *instances.at(GetKey(physicalDevice));
}
inline device& get_device(VkDevice device) {
    return *devices.at(GetKey(device));
}
inline device& get_device(VkCommandBuffer commandBuffer) {
    return *devices.at(GetKey(commandBuffer));
}

}
