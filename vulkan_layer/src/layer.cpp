#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <string.h>
#include <string>
#include <vulkan/vulkan_core.h>
#include <memory>
#include <assert.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/cfg/env.h>

#include "dispatch.hpp"
#include "constants.hpp"
#include "layer.hpp"
#include "objects.hpp"

std::mutex global_lock;
std::mutex transfer_lock;

static class spdlog_init {
	public:
		spdlog_init() {
			spdlog::cfg::load_env_levels();
		}
} init;

/*void load_plugin(std::filesystem::path path)
{
	void* h = dlopen(path.c_str(), RTLD_LAZY);
	if(!h)
	{
		*logger << logger::begin << logger::error << "dlopen failed: " << dlerror() << logger::end;
	}

	*logger << logger::begin << "Loaded plugin from " << path << logger::end;
}

void load_plugins()
{
	try
	{
		std::filesystem::directory_iterator it(global_config["pluginDirectory"]+"/");
		for(auto& f : it)
		{
			load_plugin(f.path());
		}
	}
	catch (std::filesystem::filesystem_error& ex)
	{
		*logger << logger::begin << logger::error << "Cannot find plugins: " << ex.what() << logger::end;
	}
}*/

VkLayerInstanceCreateInfo *get_chain_info(const VkInstanceCreateInfo *pCreateInfo, VkLayerFunction func) {
    VkLayerInstanceCreateInfo *chain_info = (VkLayerInstanceCreateInfo *)pCreateInfo->pNext;
    while (chain_info && !(chain_info->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && chain_info->function == func)) {
        chain_info = (VkLayerInstanceCreateInfo *)chain_info->pNext;
    }
    assert(chain_info != NULL);
    return chain_info;
}

VkLayerDeviceCreateInfo *get_chain_info(const VkDeviceCreateInfo *pCreateInfo, VkLayerFunction func) {
    VkLayerDeviceCreateInfo *chain_info = (VkLayerDeviceCreateInfo *)pCreateInfo->pNext;
    while (chain_info && !(chain_info->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && chain_info->function == func)) {
        chain_info = (VkLayerDeviceCreateInfo *)chain_info->pNext;
    }
    assert(chain_info != NULL);
    return chain_info;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
		VkInstance *pInstance)
{
    VkLayerInstanceCreateInfo* chain_info = get_chain_info(pCreateInfo, VK_LAYER_LINK_INFO);

    assert(chain_info->u.pLayerInfo);
    PFN_vkGetInstanceProcAddr fpGetInstanceProcAddr = chain_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkCreateInstance fpCreateInstance = (PFN_vkCreateInstance)fpGetInstanceProcAddr(NULL, "vkCreateInstance");
    if (fpCreateInstance == NULL) return VK_ERROR_INITIALIZATION_FAILED;
    chain_info->u.pLayerInfo = chain_info->u.pLayerInfo->pNext;

	VkResult ret = fpCreateInstance(pCreateInfo, pAllocator, pInstance);
	if(ret != VK_SUCCESS)
		return ret;

	spdlog::info("CreateInstance results in {} with dispatch key {}", fmt::ptr(*pInstance), GetKey(*pInstance));
	spdlog::default_logger()->flush();

	scoped_lock l(global_lock);
	auto& instance = CheekyLayer::instances[GetKey(*pInstance)] = std::make_unique<CheekyLayer::instance>(pCreateInfo, pInstance);
    InitInstanceDispatchTable(*pInstance, fpGetInstanceProcAddr, instance->dispatch);

	return ret;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumeratePhysicalDevices(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices)
{
	return CheekyLayer::get_instance(instance).dispatch.EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	scoped_lock l(global_lock);

	auto& inst = CheekyLayer::get_instance(instance);
	inst.logger->info("Destroying instance {}", fmt::ptr(instance));
	inst.dispatch.DestroyInstance(instance, pAllocator);
	CheekyLayer::instances.erase(GetKey(instance));
}

namespace CheekyLayer {

VkResult instance::CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) {
    VkLayerDeviceCreateInfo *chain_info = get_chain_info(pCreateInfo, VK_LAYER_LINK_INFO);

    PFN_vkGetInstanceProcAddr fpGetInstanceProcAddr = chain_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr = chain_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    PFN_vkCreateDevice fpCreateDevice = (PFN_vkCreateDevice)fpGetInstanceProcAddr(handle, "vkCreateDevice");
    if (fpCreateDevice == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    chain_info->u.pLayerInfo = chain_info->u.pLayerInfo->pNext;

	VkResult ret = fpCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
	if(ret != VK_SUCCESS)
		return ret;

	logger->debug("CreateDevice results in {} with dispatch key {}", fmt::ptr(*pDevice), GetKey(*pDevice));

	auto& dev = ::CheekyLayer::devices[GetKey(*pDevice)] = std::make_unique<device>(this, fpGetDeviceProcAddr, physicalDevice, pCreateInfo, pDevice);
	devices[*pDevice] = dev.get();
    InitDeviceDispatchTable(*pDevice, fpGetDeviceProcAddr, dev->dispatch);

	logger->flush();

	return VK_SUCCESS;
}

void device::GetDeviceQueue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue) {
	dispatch.GetDeviceQueue(handle, queueFamilyIndex, queueIndex, pQueue);
	logger->debug("GetDeviceQueue: {}, {}, {} -> {}", fmt::ptr(handle), queueFamilyIndex, queueIndex, fmt::ptr(*pQueue));

	if(!(queueFamilies[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		return;

	if(transferQueue == VK_NULL_HANDLE)
	{
		dispatch.GetDeviceQueue(handle, queueFamilyIndex, queueIndex, &transferQueue);
		*reinterpret_cast<void**>(transferQueue) = *reinterpret_cast<void**>(handle);

		VkCommandPoolCreateInfo poolInfo;
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndex;
		if(dispatch.CreateCommandPool(handle, &poolInfo, nullptr, &transferPool) != VK_SUCCESS) {
			logger->error("failed to create command pool");
			return;
		}

		VkCommandBufferAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.commandPool = transferPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = 1;
		if(dispatch.AllocateCommandBuffers(handle, &allocateInfo, &transferBuffer) != VK_SUCCESS) {
			logger->error("failed to allocate command buffer");
			return;
		}
		*reinterpret_cast<void**>(transferBuffer) = *reinterpret_cast<void**>(handle);

		logger->debug("Created transfer command buffer {} in pool {} for queue family {} on device {}", fmt::ptr(transferBuffer), fmt::ptr(transferPool), queueFamilyIndex, fmt::ptr(handle));

		rules::calling_context ctx{};
		execute_rules(rules::selector_type::DeviceCreate, (rules::VkHandle) handle, ctx);
	}
}

}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
	return CheekyLayer::get_instance(physicalDevice).CreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_GetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties)
{
	*pQueueFamilyPropertyCount = 1;
	CheekyLayer::get_instance(physicalDevice).dispatch.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
	*pQueueFamilyPropertyCount = 1;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_GetPhysicalDeviceQueueFamilyProperties2(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2*                   pQueueFamilyProperties)
{
	*pQueueFamilyPropertyCount = 1;
	CheekyLayer::get_instance(physicalDevice).dispatch.GetPhysicalDeviceQueueFamilyProperties2(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
	*pQueueFamilyPropertyCount = 1;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_GetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueFamilyIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	return CheekyLayer::get_device(device).GetDeviceQueue(queueFamilyIndex, queueIndex, pQueue);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	CheekyLayer::get_device(device).inst->devices.erase(device);
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
		return CheekyLayer::get_instance(physicalDevice).dispatch.EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
	}

	// don't expose any extensions
	if (pPropertyCount)
		*pPropertyCount = 0;
	return VK_SUCCESS;
}
