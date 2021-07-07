#include <ostream>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vk_layer_dispatch_table.h>
#include <vulkan/vulkan_core.h>

#include <map>
#include <string>

#include "dispatch.hpp"
#include "layer.hpp"

std::map<void*, VkLayerInstanceDispatchTable> instance_dispatch;
std::map<void*, VkLayerDispatchTable> device_dispatch;

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL CheekyLayer_GetDeviceProcAddr(VkDevice device, const char *pName)
{
	DeviceHook(DestroyDevice);

	if(!layer_disabled)
	{
		DeviceHooks();
	}

	{
		scoped_lock l(global_lock);
		return device_dispatch[GetKey(device)].GetDeviceProcAddr(device, pName);
	}
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL CheekyLayer_GetInstanceProcAddr(VkInstance instance, const char *pName)
{
	InstanceHook(CreateInstance);
	InstanceHook(CreateDevice);
	InstanceHook(DestroyInstance);

	if(!layer_disabled)
	{
		InstanceHooks();
	}

	{
		scoped_lock l(global_lock);
		return instance_dispatch[GetKey(instance)].GetInstanceProcAddr(instance, pName);
	}
}

void InitInstanceDispatchTable(VkInstance instance, PFN_vkGetInstanceProcAddr gpa)
{
	VkLayerInstanceDispatchTable dispatchTable;
	
	InitInstanceDispatch();

	{
		scoped_lock l(global_lock);
		instance_dispatch[GetKey(instance)] = dispatchTable;
	}
}

void InitDeviceDispatchTable(VkDevice device, PFN_vkGetDeviceProcAddr gdpa)
{
	VkLayerDispatchTable dispatchTable;
	
	InitDeviceDispatch();

	{
		scoped_lock l(global_lock);
		device_dispatch[GetKey(device)] = dispatchTable;
	}
}
