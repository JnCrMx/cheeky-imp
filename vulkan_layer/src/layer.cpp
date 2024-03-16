#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <string.h>
#include <string>
#include <vulkan/vulkan_core.h>
#include <memory>
#include <filesystem>
#include <assert.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "config.hpp"
#include "dispatch.hpp"
#include "constants.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "rules/reader.hpp"
#include "draw.hpp"
#include "objects.hpp"

using CheekyLayer::logger;

std::mutex global_lock;
std::mutex transfer_lock;
CheekyLayer::config global_config;
CheekyLayer::logger* logger;
std::vector<std::string> overrideCache;
std::vector<std::string> dumpCache;

std::vector<std::unique_ptr<CheekyLayer::rules::rule>> rules;
std::map<CheekyLayer::rules::selector_type, bool> has_rules;

std::map<VkDevice, VkQueue> graphicsQueues;
std::map<VkDevice, VkQueue> transferQueues;
std::map<VkDevice, VkCommandPool> transferPools;
std::map<VkDevice, VkCommandBuffer> transferCommandBuffers;

std::map<VkDevice, VkPhysicalDevice> physicalDevices;
std::map<VkDevice, VkDeviceInfo> deviceInfos;

std::map<VkQueue, VkDevice> queueDevices;

void update_has_rules()
{
	has_rules[CheekyLayer::rules::selector_type::Buffer] = false;
	has_rules[CheekyLayer::rules::selector_type::Draw] = false;
	has_rules[CheekyLayer::rules::selector_type::Image] = false;
	has_rules[CheekyLayer::rules::selector_type::Shader] = false;

	for(auto& r : rules)
	{
		if(r->is_enabled())
			has_rules[r->get_type()] = true;
	}

	evalRulesInDraw = has_rules[CheekyLayer::rules::selector_type::Draw];
}

void load_plugin(std::filesystem::path path)
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
}

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

	/*global_config = CheekyLayer::DEFAULT_CONFIG;
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

	std::string applicationName = "unknown";
	std::string engineName = "unknown";
	if(pCreateInfo->pApplicationInfo && pCreateInfo->pApplicationInfo->pApplicationName)
		applicationName = pCreateInfo->pApplicationInfo->pApplicationName;
	if(pCreateInfo->pApplicationInfo && pCreateInfo->pApplicationInfo->pEngineName)
		engineName = pCreateInfo->pApplicationInfo->pEngineName;

	std::string logfile = global_config["logFile"];
	replace(logfile, "{{pid}}", std::to_string(getpid()));
	replace(logfile, "{{inst}}", std::to_string((uint64_t)*pInstance));
	if(!global_config["application"].empty())
	{
		if(global_config["application"] != applicationName)
		{
			layer_disabled = true;
			//logfile = "/dev/null";
		} else {
			layer_disabled = false;
		}
	}
	hook_draw_calls = global_config.map<bool>("hookDraw", CheekyLayer::config::to_bool);

	std::ofstream out(logfile);
	logger = new CheekyLayer::logger(out);

	static unsigned int logger_id = 0;
	auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>("instance #"+std::to_string(logger_id++), logfile+".spdlog");
	spdlog::set_default_logger(async_file);
	spdlog::info("Hello from {} version {} from {} {} for application {} using engine {}",
		CheekyLayer::Contants::LAYER_NAME, CheekyLayer::Contants::GIT_VERSION,
		__DATE__, __TIME__, applicationName, engineName);
	spdlog::flush_every(std::chrono::seconds(3));

	*logger << logger::begin << "Hello from " << CheekyLayer::Contants::LAYER_NAME
		<< " version " << CheekyLayer::Contants::GIT_VERSION
		<< " from " << __DATE__ << " " << __TIME__
		<< " for application " << applicationName
		<< " using engine " << engineName << logger::end;
	*logger << logger::begin << "CreateInstance: " << *pInstance << " -> " << ret << logger::end;

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
			*logger << logger::begin << logger::error << "Cannot find overrides for " << type << ": " << ex.what() << logger::end;
		}

		/*if(global_config.map<bool>("dump", CheekyLayer::config::to_bool))
		{
			try
			{
				std::filesystem::directory_iterator it(global_config["dumpDirectory"]+"/"+type);
				int count = 0;
				std::transform(std::filesystem::begin(it), std::filesystem::end(it), std::back_inserter(dumpCache), [&count](auto e){
					count++;
					return e.path().stem();
				});
				*logger << logger::begin << "Found " << count << " dumped resources for " << type << logger::end;
				//dumpCache.reserve(dumpCache.size()*2);
			}
			catch (std::filesystem::filesystem_error& ex)
			{
				*logger << logger::begin << logger::error << "Cannot find dumped resources for " << type << ": " << ex.what() << logger::end;
			}
		}*/
	/*}
	load_plugins();

	std::string rulefile = global_config["ruleFile"];
	rules.clear();

	{
		std::ifstream rulesIn(rulefile);
		CheekyLayer::rules::numbered_streambuf numberer{rulesIn};
		while(rulesIn.good())
		{
			try
			{
				std::unique_ptr<CheekyLayer::rules::rule> rule = std::make_unique<CheekyLayer::rules::rule>();
				rulesIn >> *rule;

				rules.push_back(std::move(rule));
			}
			catch(const std::exception& ex)
			{
				*logger << logger::begin << "Error at " << numberer.line() << ":" << numberer.col() << ":\n\t" << ex.what() << logger::end;
				break;
			}
		}
	}

	*logger << logger::begin << "Loaded " << rules.size() << " rules:" << logger::end;
	for(auto& r : rules)
	{
		CheekyLayer::active_logger log = *logger << logger::begin;
		log << '\t';
		r->print(log.raw());
		log << logger::end;
	}
	CheekyLayer::rules::rule_disable_callback = [](CheekyLayer::rules::rule* rule){
		update_has_rules();
	};
	update_has_rules();

	// close all file descriptors when creating a new instance
	for(auto& [name, fd] : CheekyLayer::rules::rule_env.fds)
	{
		fd->close();
	}
	CheekyLayer::rules::rule_env.fds.clear();

	if(!layer_disabled)
	{
		CheekyLayer::active_logger log = *logger << logger::begin;
		CheekyLayer::rules::local_context ctx = { .logger = log };
		CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::Init, VK_NULL_HANDLE, ctx);
		log << logger::end;
	}*/

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

	logger->info("CreateDevice results in {} with dispatch key {}", fmt::ptr(*pDevice), GetKey(*pDevice));

	auto& dev = ::CheekyLayer::devices[GetKey(*pDevice)] = std::make_unique<device>(this, fpGetDeviceProcAddr, physicalDevice, pCreateInfo, pDevice);
	devices[*pDevice] = dev.get();
    InitDeviceDispatchTable(*pDevice, fpGetDeviceProcAddr, dev->dispatch);

	logger->flush();

	return VK_SUCCESS;
}

void device::GetDeviceQueue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue) {
	dispatch.GetDeviceQueue(handle, queueFamilyIndex, queueIndex, pQueue);
	logger->info("GetDeviceQueue: {}, {}, {} -> {}", fmt::ptr(handle), queueFamilyIndex, queueIndex, fmt::ptr(*pQueue));

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

		logger->info("Created transfer command buffer {} in pool {} for queue family {} on device {}", fmt::ptr(transferBuffer), fmt::ptr(transferPool), queueFamilyIndex, fmt::ptr(handle));

		/*
		// now we are "ready"
		CheekyLayer::active_logger log = *logger << logger::begin;
		CheekyLayer::rules::local_context ctx = { .logger = log, .device = device };
		CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::DeviceCreate, (VkHandle) device, ctx);
		log << logger::end;
		*/
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

	/*scoped_lock l(global_lock);
	if(logger)
		*logger << logger::begin << "DestroyDevice: " << device << logger::end;

	if(transferQueues.contains(device))
	{
		device_dispatch[GetKey(device)].DestroyCommandPool(device, transferPools[device], nullptr);
		transferCommandBuffers.erase(device);
		transferPools.erase(device);
		transferQueues.erase(device);
	}

	if(!layer_disabled)
	{
		CheekyLayer::active_logger log = *logger << logger::begin;
		CheekyLayer::rules::local_context ctx = { .logger = log };
		CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::DeviceDestroy, (VkHandle) device, ctx);
		log << logger::end;
	}

	device_dispatch[GetKey(device)].DestroyDevice(device, pAllocator);

	device_dispatch.erase(GetKey(device));*/
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
