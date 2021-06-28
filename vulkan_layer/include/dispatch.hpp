#pragma once

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vk_layer_dispatch_table.h>

#include <map>
#include <string.h>

template<typename DispatchableType>
void* GetKey(DispatchableType inst)
{
	return *(void**) inst;
}

extern std::map<void*, VkLayerInstanceDispatchTable> instance_dispatch;
extern std::map<void*, VkLayerDispatchTable> device_dispatch;

void InitInstanceDispatchTable(VkInstance instance, PFN_vkGetInstanceProcAddr gpa);
void InitDeviceDispatchTable(VkDevice device, PFN_vkGetDeviceProcAddr gdpa);

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
	InstanceHook(CreateDevice)

#define DeviceHooks() \
	DeviceHook(GetDeviceProcAddr) \
	DeviceHook(DestroyDevice) \
	\
	DeviceHook(CreateImage) \
	DeviceHook(BindImageMemory) \
	\
	DeviceHook(CreateBuffer) \
	DeviceHook(BindBufferMemory) \
	DeviceHook(MapMemory) \
	\
	DeviceHook(CreateShaderModule) \
	\
	DeviceHook(CmdCopyBufferToImage) \
	DeviceHook(CmdCopyBuffer)

#define InstanceDispatch(name) \
	dispatchTable.name = (PFN_vk##name)gpa(instance, "vk"#name);
#define DeviceDispatch(name) \
	dispatchTable.name = (PFN_vk##name)gdpa(device, "vk"#name);

#define InitInstanceDispatch() \
	InstanceDispatch(GetInstanceProcAddr) \
	InstanceDispatch(DestroyInstance) \
	InstanceDispatch(EnumerateDeviceExtensionProperties) \
	InstanceDispatch(GetPhysicalDeviceMemoryProperties) \
	InstanceDispatch(GetPhysicalDeviceProperties)

#define InitDeviceDispatch() \
	DeviceDispatch(GetDeviceProcAddr) \
	DeviceDispatch(DestroyDevice) \
	\
	DeviceDispatch(CreateShaderModule) \
	\
	DeviceDispatch(CreateImage) \
	DeviceDispatch(GetImageMemoryRequirements) \
	DeviceDispatch(BindImageMemory)\
	\
	DeviceDispatch(CreateBuffer) \
	DeviceDispatch(BindBufferMemory) \
	DeviceDispatch(MapMemory) \
	DeviceDispatch(UnmapMemory) \
	\
	DeviceDispatch(CreateShaderModule) \
	\
	DeviceDispatch(CmdCopyBufferToImage) \
	DeviceDispatch(CmdCopyBuffer)
