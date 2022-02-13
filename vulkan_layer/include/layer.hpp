#pragma once

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vk_layer_dispatch_table.h>

#include <mutex>
#include <memory>
#include <map>
#include <vulkan/vulkan_core.h>

#include "config.hpp"
#include "logger.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"

#undef VK_LAYER_EXPORT
#if defined(WIN32)
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#else
#define VK_LAYER_EXPORT extern "C"
#endif

extern std::mutex global_lock;
extern std::mutex transfer_lock;
typedef std::lock_guard<std::mutex> scoped_lock;

extern CheekyLayer::config global_config;
extern CheekyLayer::logger* logger;

extern std::vector<std::string> overrideCache;

extern std::vector<std::unique_ptr<CheekyLayer::rules::rule>> rules;
extern std::map<CheekyLayer::rules::selector_type, bool> has_rules;

extern std::map<VkDevice, VkQueue> transferQueues;
extern std::map<VkDevice, VkCommandPool> transferPools;
extern std::map<VkDevice, VkCommandBuffer> transferCommandBuffers;

extern std::map<VkQueue, VkDevice> queueDevices;

struct VkDeviceInfo {
    VkPhysicalDeviceProperties general;
    VkPhysicalDeviceMemoryProperties memory;
    std::vector<VkQueueFamilyProperties> queueFamilies;
};
extern std::map<VkDevice, VkDeviceInfo> deviceInfos;

// layer.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyInstance(VkInstance, const VkAllocationCallbacks*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_GetDeviceQueue(VkDevice, uint32_t, uint32_t, VkQueue*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyDevice(VkDevice, const VkAllocationCallbacks*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateInstanceLayerProperties(uint32_t*, VkLayerProperties*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateDeviceLayerProperties(VkPhysicalDevice, uint32_t*, VkLayerProperties*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateInstanceExtensionProperties(const char*, uint32_t*, VkExtensionProperties*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateDeviceExtensionProperties(VkPhysicalDevice, const char*, uint32_t*, VkExtensionProperties*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumeratePhysicalDevices(VkInstance, uint32_t*, VkPhysicalDevice*);

// dispatch.cpp
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL CheekyLayer_GetDeviceProcAddr(VkDevice, const char*);
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL CheekyLayer_GetInstanceProcAddr(VkInstance, const char*);

// images.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_BindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*);

// buffers.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateBuffer(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_BindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_MapMemory(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*);

// shaders.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateShaderModule(VkDevice, VkShaderModuleCreateInfo*, VkAllocationCallbacks*, VkShaderModule*);

// descriptors.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateDescriptorUpdateTemplate(VkDevice, const VkDescriptorUpdateTemplateCreateInfo*, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_UpdateDescriptorSetWithTemplate(VkDevice, VkDescriptorSet, VkDescriptorUpdateTemplate, const void*);

// draw.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_AllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_FreeCommandBuffers(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateFramebuffer(VkDevice, const VkFramebufferCreateInfo*, const VkAllocationCallbacks*, VkFramebuffer*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateGraphicsPipelines(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindVertexBuffers(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindVertexBuffers2EXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*, const VkDeviceSize*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindIndexBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdSetScissor(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBeginRenderPass(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdEndRenderPass(VkCommandBuffer);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdDrawIndexed(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdDraw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBeginTransformFeedbackEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdEndTransformFeedbackEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EndCommandBuffer(VkCommandBuffer);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_QueuePresentKHR(VkQueue, const VkPresentInfoKHR*);
