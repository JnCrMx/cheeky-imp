#pragma once

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <vulkan/generated/vk_layer_dispatch_table.h>

#include <map>
#include <string.h>

#include "layer.hpp"

template<typename DispatchableType>
void* GetKey(DispatchableType inst)
{
	return *(void**) inst;
}

extern std::map<void*, VkLayerInstanceDispatchTable> instance_dispatch;
extern std::map<void*, VkLayerDispatchTable> device_dispatch;

inline bool layer_disabled = false;
inline bool hook_draw_calls = false;

void InitInstanceDispatchTable(VkInstance instance, PFN_vkGetInstanceProcAddr gpa, VkLayerInstanceDispatchTable& dispatchTable);
void InitDeviceDispatchTable(VkDevice device, PFN_vkGetDeviceProcAddr gdpa, VkLayerDispatchTable& dispatchTable);

#define InstanceHook(func) \
	(void)((VkLayerInstanceDispatchTable*)0)->func; \
	if(!strcmp(pName, "vk" #func)) return (PFN_vkVoidFunction)&CheekyLayer_##func;
#define DeviceHook(func) \
	(void)((VkLayerDispatchTable*)0)->func; \
	if(!strcmp(pName, "vk" #func)) return (PFN_vkVoidFunction)&CheekyLayer_##func;

#define InstanceHooks() \
	InstanceHook(GetInstanceProcAddr) \
	InstanceHook(EnumerateInstanceLayerProperties) \
	InstanceHook(EnumerateInstanceExtensionProperties) \
	InstanceHook(CreateInstance) \
	InstanceHook(DestroyInstance) \
	InstanceHook(EnumerateDeviceLayerProperties) \
	InstanceHook(EnumerateDeviceExtensionProperties) \
	InstanceHook(CreateDevice) \
	InstanceHook(EnumeratePhysicalDevices) \
	InstanceHook(GetPhysicalDeviceQueueFamilyProperties) \
	InstanceHook(GetPhysicalDeviceQueueFamilyProperties2)

#define DeviceHooks() \
	DeviceHook(GetDeviceProcAddr) \
	DeviceHook(DestroyDevice) \
	\
	DeviceHook(GetDeviceQueue) \
	\
	DeviceHook(CreateImage) \
	DeviceHook(BindImageMemory) \
	DeviceHook(CreateImageView) \
	\
	DeviceHook(CreateBuffer) \
	DeviceHook(BindBufferMemory) \
	DeviceHook(MapMemory) \
	DeviceHook(UnmapMemory) \
	\
	DeviceHook(CreateShaderModule) \
	DeviceHook(CreateGraphicsPipelines) \
	DeviceHook(CreatePipelineLayout) \
	\
	DeviceHook(CmdCopyBufferToImage) \
	DeviceHook(CmdCopyBuffer) \
	\
	DeviceHook(CreateFramebuffer) \
	DeviceHook(CreateSwapchainKHR) \
	DeviceHook(QueuePresentKHR) \
	\
	if(hook_draw_calls) \
	{\
		DeviceHook(CmdBindDescriptorSets) \
		DeviceHook(CmdBindPipeline) \
		DeviceHook(CmdBindVertexBuffers) \
		DeviceHook(CmdBindVertexBuffers2EXT) \
		DeviceHook(CmdBindIndexBuffer) \
		DeviceHook(CmdSetScissor) \
		DeviceHook(CmdBeginRenderPass) \
		DeviceHook(CmdEndRenderPass) \
		DeviceHook(CmdDraw) \
		DeviceHook(CmdDrawIndexed) \
		DeviceHook(CmdBeginTransformFeedbackEXT) \
		DeviceHook(CmdBindTransformFeedbackBuffersEXT) \
		DeviceHook(CmdEndTransformFeedbackEXT) \
		\
		DeviceHook(CreateDescriptorUpdateTemplate) \
		DeviceHook(UpdateDescriptorSetWithTemplate) \
		\
		DeviceHook(AllocateCommandBuffers) \
		DeviceHook(FreeCommandBuffers) \
		DeviceHook(EndCommandBuffer) \
		DeviceHook(QueueSubmit) \
	}\

#define InstanceDispatch(name) \
	dispatchTable.name = (PFN_vk##name)gpa(instance, "vk"#name);
#define DeviceDispatch(name) \
	dispatchTable.name = (PFN_vk##name)gdpa(device, "vk"#name);

#define InitInstanceDispatch() \
	InstanceDispatch(GetInstanceProcAddr) \
	InstanceDispatch(DestroyInstance) \
	InstanceDispatch(EnumerateDeviceExtensionProperties) \
	InstanceDispatch(GetPhysicalDeviceMemoryProperties) \
	InstanceDispatch(GetPhysicalDeviceProperties) \
	InstanceDispatch(GetPhysicalDeviceQueueFamilyProperties) \
	InstanceDispatch(GetPhysicalDeviceQueueFamilyProperties2) \
	InstanceDispatch(EnumeratePhysicalDevices) \
	\
	InstanceDispatch(DestroySurfaceKHR) \
	InstanceDispatch(GetPhysicalDeviceSurfaceCapabilitiesKHR) \
	InstanceDispatch(GetPhysicalDeviceSurfaceFormatsKHR) \
	InstanceDispatch(GetPhysicalDeviceSurfacePresentModesKHR)

#define InitDeviceDispatch() \
	DeviceDispatch(GetDeviceProcAddr) \
	DeviceDispatch(DestroyDevice) \
	DeviceDispatch(SetDebugUtilsObjectNameEXT) \
	\
	DeviceDispatch(GetDeviceQueue) \
	DeviceDispatch(CreateCommandPool) \
	DeviceDispatch(DestroyCommandPool) \
	DeviceDispatch(QueueSubmit) \
	DeviceDispatch(QueueWaitIdle) \
	\
	DeviceDispatch(CreateShaderModule) \
	\
	DeviceDispatch(CreateImage) \
	DeviceDispatch(GetImageMemoryRequirements) \
	DeviceDispatch(BindImageMemory) \
	DeviceDispatch(CreateImageView) \
	DeviceDispatch(CreateSampler) \
	\
	DeviceDispatch(CreateBuffer) \
	DeviceDispatch(DestroyBuffer) \
	DeviceDispatch(GetBufferMemoryRequirements) \
	DeviceDispatch(BindBufferMemory) \
	DeviceDispatch(AllocateMemory) \
	DeviceDispatch(FreeMemory) \
	DeviceDispatch(MapMemory) \
	DeviceDispatch(UnmapMemory) \
	DeviceDispatch(FlushMappedMemoryRanges) \
	\
	DeviceDispatch(CreateShaderModule) \
	DeviceDispatch(CreateGraphicsPipelines) \
	DeviceDispatch(CreatePipelineLayout) \
	DeviceDispatch(CreateRenderPass) \
	\
	DeviceDispatch(CreateDescriptorSetLayout) \
	DeviceDispatch(CreateDescriptorPool) \
	DeviceDispatch(AllocateDescriptorSets) \
	\
	DeviceDispatch(CmdCopyBufferToImage) \
	DeviceDispatch(CmdCopyBuffer) \
	DeviceDispatch(CmdBindDescriptorSets) \
	DeviceDispatch(CmdBindPipeline) \
	DeviceDispatch(CmdBindVertexBuffers) \
	DeviceDispatch(CmdBindVertexBuffers2EXT) \
	DeviceDispatch(CmdBindIndexBuffer) \
	DeviceDispatch(CmdSetScissor) \
	DeviceDispatch(CmdDrawIndexed) \
	DeviceDispatch(CmdDraw) \
	DeviceDispatch(CmdPipelineBarrier) \
	DeviceDispatch(CmdBeginRenderPass) \
	DeviceDispatch(CmdEndRenderPass) \
	DeviceDispatch(CmdBeginTransformFeedbackEXT) \
	DeviceDispatch(CmdBindTransformFeedbackBuffersEXT) \
	DeviceDispatch(CmdEndTransformFeedbackEXT) \
	DeviceDispatch(CmdPushConstants) \
	\
	DeviceDispatch(CreateDescriptorUpdateTemplate) \
	DeviceDispatch(UpdateDescriptorSetWithTemplate) \
	DeviceDispatch(UpdateDescriptorSets) \
	\
	DeviceDispatch(AllocateCommandBuffers) \
	DeviceDispatch(FreeCommandBuffers) \
	DeviceDispatch(ResetCommandBuffer) \
	DeviceDispatch(BeginCommandBuffer) \
	DeviceDispatch(EndCommandBuffer) \
	\
	DeviceDispatch(CreateSwapchainKHR) \
	DeviceDispatch(QueuePresentKHR) \
	DeviceDispatch(GetSwapchainImagesKHR) \
	DeviceDispatch(CreateSemaphore) \
	\
	DeviceDispatch(CreateFramebuffer) \
	DeviceDispatch(CreateEvent) \
	DeviceDispatch(DestroyEvent) \
	DeviceDispatch(CmdSetEvent) \
	DeviceDispatch(GetEventStatus) \
	DeviceDispatch(CmdCopyImageToBuffer) \
	\
	DeviceDispatch(CreateFence) \
	DeviceDispatch(ResetFences) \
	DeviceDispatch(WaitForFences)
