#include <algorithm>
#include <exception>
#include <iterator>
#include <mutex>
#include <fstream>
#include <string.h>
#include <vulkan/vulkan_core.h>
#include <memory>
#include <filesystem>

#include "config.hpp"
#include "dispatch.hpp"
#include "constants.hpp"
#include "layer.hpp"
#include "logger.hpp"

using CheekyLayer::logger;

std::mutex global_lock;
CheekyLayer::config global_config;
CheekyLayer::logger* logger;
std::vector<std::string> overrideCache;

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
		VkInstance *pInstance)
{
	VkLayerInstanceCreateInfo *layerCreateInfo = (VkLayerInstanceCreateInfo*) pCreateInfo->pNext;

	// step through the chain of pNext until we get to the link info
	while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO))
	{
		layerCreateInfo = (VkLayerInstanceCreateInfo*) layerCreateInfo->pNext;
	}

	if (layerCreateInfo == NULL)
	{
		// No loader instance create info
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	// move chain on for next layer
	layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

	PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance) gpa(VK_NULL_HANDLE, "vkCreateInstance");
	VkResult ret = createFunc(pCreateInfo, pAllocator, pInstance);

	InitInstanceDispatchTable(*pInstance, gpa);

	global_config = CheekyLayer::DEFAULT_CONFIG;
	try 
	{
		std::string configFile = std::getenv(CheekyLayer::Contants::CONFIG_ENV.c_str());
		std::ifstream in(configFile);
		if(in.good())
		{
			in >> global_config;
		}
	}
	catch(std::exception&) {}

	std::ofstream out(global_config["logFile"]);
	logger = new CheekyLayer::logger(out);

	*logger << logger::begin << "Hello from " << CheekyLayer::Contants::LAYER_NAME << logger::end;
	*logger << logger::begin << "CreateInstance: " << *pInstance << logger::end;

	for(std::string type : {"images", "buffers", "shaders"})
	{
		try
		{
			std::filesystem::directory_iterator it(global_config["overrideDirectory"]+"/"+type);

			int count = 0;
			std::transform(std::filesystem::begin(it), std::filesystem::end(it), std::back_inserter(overrideCache), [&count](auto e){
				count++;
				return e.path().stem();
			});
			*logger << logger::begin << "Found " << count << " overrides for " << type << logger::end;
		}
		catch (std::filesystem::filesystem_error& ex)
		{
			*logger << logger::begin << "Cannot find overrides for " << type << ": " << ex.what() << logger::end;
		}
	}

	return ret;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	//instance_dispatch[GetKey(instance)].DestroyInstance(instance, pAllocator);

	scoped_lock l(global_lock);

	if(instance_dispatch.find(GetKey(instance)) != instance_dispatch.end())
		instance_dispatch.erase(GetKey(instance));

	*logger << logger::begin << "DestroyInstance: " << instance << logger::end;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	VkLayerDeviceCreateInfo *layerCreateInfo = (VkLayerDeviceCreateInfo*) pCreateInfo->pNext;

	// step through the chain of pNext until we get to the link info
	while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO))
	{
		layerCreateInfo = (VkLayerDeviceCreateInfo*) layerCreateInfo->pNext;
	}

	if (layerCreateInfo == NULL)
	{
		// No loader instance create info
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
	// move chain on for next layer
	layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

	PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice) gipa(VK_NULL_HANDLE, "vkCreateDevice");

	VkResult ret = createFunc(physicalDevice, pCreateInfo, pAllocator, pDevice);

	InitDeviceDispatchTable(*pDevice, gdpa);

	VkPhysicalDeviceProperties props;
	instance_dispatch.begin()->second.GetPhysicalDeviceProperties(physicalDevice, &props);
	*logger << logger::begin << "CreateDevice: " << *pDevice << " for " << props.deviceName << logger::end;

	return ret;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	device_dispatch[GetKey(device)].DestroyDevice(device, pAllocator);

	scoped_lock l(global_lock);
	device_dispatch.erase(GetKey(device));

	if(logger)
		*logger << logger::begin << "DestroyDevice: " << device << logger::end;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateInstanceLayerProperties(uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
	if(pPropertyCount)
		*pPropertyCount = 1;

	if (pProperties)
	{
		strcpy(pProperties->layerName, CheekyLayer::Contants::LAYER_NAME.c_str());
		strcpy(pProperties->description, CheekyLayer::Contants::LAYER_DESCRIPTION.c_str());
		pProperties->implementationVersion = 1;
		pProperties->specVersion = VK_API_VERSION_1_1;
	}

	return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount,
		VkLayerProperties *pProperties)
{
	return CheekyLayer_EnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateInstanceExtensionProperties(const char *pLayerName, uint32_t *pPropertyCount,
		VkExtensionProperties *pProperties)
{
	if (pLayerName == NULL || strcmp(pLayerName, CheekyLayer::Contants::LAYER_NAME.c_str()))
		return VK_ERROR_LAYER_NOT_PRESENT;

	// don't expose any extensions
	if (pPropertyCount)
		*pPropertyCount = 0;
	return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char *pLayerName,
		uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
	// pass through any queries that aren't to us
	if (pLayerName == NULL || strcmp(pLayerName, CheekyLayer::Contants::LAYER_NAME.c_str()))
	{
		if (physicalDevice == VK_NULL_HANDLE)
			return VK_SUCCESS;

		scoped_lock l(global_lock);
		return instance_dispatch[GetKey(physicalDevice)].EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
	}

	// don't expose any extensions
	if (pPropertyCount)
		*pPropertyCount = 0;
	return VK_SUCCESS;
}
