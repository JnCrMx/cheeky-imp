#pragma once

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vk_layer_dispatch_table.h>

#include <mutex>
#include <memory>

#include "config.hpp"
#include "logger.hpp"

#undef VK_LAYER_EXPORT
#if defined(WIN32)
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#else
#define VK_LAYER_EXPORT extern "C"
#endif

extern std::mutex global_lock;
typedef std::lock_guard<std::mutex> scoped_lock;

extern CheekyLayer::config global_config;
extern CheekyLayer::logger* logger;

extern std::vector<std::string> overrideCache;

// layer.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyInstance(VkInstance, const VkAllocationCallbacks*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyDevice(VkDevice, const VkAllocationCallbacks*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateInstanceLayerProperties(uint32_t*, VkLayerProperties*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateDeviceLayerProperties(VkPhysicalDevice, uint32_t*, VkLayerProperties*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateInstanceExtensionProperties(const char*, uint32_t*, VkExtensionProperties*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateDeviceExtensionProperties(VkPhysicalDevice, const char*, uint32_t*, VkExtensionProperties*);

// dispatch.cpp
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL CheekyLayer_GetDeviceProcAddr(VkDevice, const char*);
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL CheekyLayer_GetInstanceProcAddr(VkInstance, const char*);

// images.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_BindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*);

// buffers.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateBuffer(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_BindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_MapMemory(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**);
VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*);

// shaders.cpp
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateShaderModule(VkDevice, VkShaderModuleCreateInfo*, VkAllocationCallbacks*, VkShaderModule*);
