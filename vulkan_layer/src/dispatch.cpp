#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <vulkan/utility/vk_dispatch_table.h>
#include <vulkan/vulkan_core.h>
#include <spdlog/spdlog.h>

#include <map>

#include "dispatch.hpp"
#include "layer.hpp"
#include "objects.hpp"

std::map<void*, VkuInstanceDispatchTable> instance_dispatch;
std::map<void*, VkuDeviceDispatchTable> device_dispatch;

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL CheekyLayer_GetDeviceProcAddr(VkDevice device, const char *pName)
{
	DeviceHook(DestroyDevice);

	if(!layer_disabled)
	{
		DeviceHooks();
	}

	{
		scoped_lock l(global_lock);
		return CheekyLayer::get_device(device).dispatch.GetDeviceProcAddr(device, pName);
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
		return CheekyLayer::get_instance(instance).dispatch.GetInstanceProcAddr(instance, pName);
	}
}

void InitInstanceDispatchTable(VkInstance instance, PFN_vkGetInstanceProcAddr gpa, VkuInstanceDispatchTable& dispatchTable)
{
	InitInstanceDispatch();
}

void InitDeviceDispatchTable(VkDevice device, PFN_vkGetDeviceProcAddr gdpa, VkuDeviceDispatchTable_& dispatchTable)
{
	InitDeviceDispatch();
}
