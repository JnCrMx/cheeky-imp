#include <algorithm>
#include <cstring>
#include <dlfcn.h>
#include <exception>
#include <iterator>
#include <mutex>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <string>
#include <vulkan/vulkan_core.h>
#include <memory>
#include <filesystem>
#include <assert.h>

#include "config.hpp"
#include "dispatch.hpp"
#include "constants.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "rules/reader.hpp"
#include "draw.hpp"
#include "utils.hpp"

using CheekyLayer::logger;

std::mutex global_lock;
std::mutex transfer_lock;
CheekyLayer::config global_config;
CheekyLayer::logger* logger;
std::vector<std::string> overrideCache;
std::vector<std::string> dumpCache;

std::vector<VkInstance> instances;
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

	if(!instances.empty())
	{
		*logger << logger::begin << "CreateInstance: " << *pInstance << " -> " << ret << logger::end;
		
		instances.push_back(*pInstance);
		InitInstanceDispatchTable(*pInstance, fpGetInstanceProcAddr);

		return ret;
	}

	instances.push_back(*pInstance);
	InitInstanceDispatchTable(*pInstance, fpGetInstanceProcAddr);

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
			logfile = "/dev/null";
		}
	}
	hook_draw_calls = global_config.map<bool>("hookDraw", CheekyLayer::config::to_bool);

	std::ofstream out(logfile);
	logger = new CheekyLayer::logger(out);

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
	}
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
	}

	return ret;
}

std::map<VkPhysicalDevice, VkInstance> physicalDeviceInstances;
VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EnumeratePhysicalDevices(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices)
{
	VkResult r = instance_dispatch[GetKey(instance)].EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);

	if(pPhysicalDeviceCount && pPhysicalDevices)
	{
		for(int i=0; i<*pPhysicalDeviceCount; i++)
		{
			physicalDeviceInstances[pPhysicalDevices[i]] = instance;
		}
	}

	return r;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
	scoped_lock l(global_lock);

	auto p = std::find(instances.begin(), instances.end(), instance);
	if(p != instances.end())
	{
		instances.erase(p);
	}

	instance_dispatch[GetKey(instance)].DestroyInstance(instance, pAllocator);
	instance_dispatch.erase(GetKey(instance));

	*logger << logger::begin << "DestroyInstance: " << instance << logger::end;

	/*for(auto& [name, fd] : CheekyLayer::rules::rule_env.fds)
	{
		fd->close();
	}
	CheekyLayer::rules::rule_env.fds.clear();

	for(auto& t : CheekyLayer::rules::rule_env.threads)
		t.join();
	CheekyLayer::rules::rule_env.threads.clear();*/
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
    VkLayerDeviceCreateInfo *chain_info = get_chain_info(pCreateInfo, VK_LAYER_LINK_INFO);

    PFN_vkGetInstanceProcAddr fpGetInstanceProcAddr = chain_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr = chain_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    PFN_vkCreateDevice fpCreateDevice = (PFN_vkCreateDevice)fpGetInstanceProcAddr(physicalDeviceInstances[physicalDevice], "vkCreateDevice");
    if (fpCreateDevice == NULL) {
		std::abort();
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    chain_info->u.pLayerInfo = chain_info->u.pLayerInfo->pNext;

	VkResult ret = fpCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
	if(ret != VK_SUCCESS)
		return ret;

	InitDeviceDispatchTable(*pDevice, fpGetDeviceProcAddr);

	VkPhysicalDeviceProperties props;
	instance_dispatch.begin()->second.GetPhysicalDeviceProperties(physicalDevice, &props);
	*logger << logger::begin << "CreateDevice: " << *pDevice << " for " << props.deviceName << " -> " << ret <<  " with " << pCreateInfo->enabledLayerCount << " layers:" << logger::end;
	for(int i=0; i<pCreateInfo->enabledLayerCount; i++)
	{
		*logger << logger::begin << '\t' << pCreateInfo->ppEnabledLayerNames[i] << logger::end;
	}
	*logger << logger::begin << "and " << pCreateInfo->enabledExtensionCount << " extensions:" << logger::end;
	for(int i=0; i<pCreateInfo->enabledExtensionCount; i++)
	{
		*logger << logger::begin << '\t' << pCreateInfo->ppEnabledExtensionNames[i] << logger::end;
	}

	VkPhysicalDeviceMemoryProperties memProperties;
	instance_dispatch.begin()->second.GetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	uint32_t count;
	instance_dispatch.begin()->second.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
	std::vector<VkQueueFamilyProperties> families(count);
	instance_dispatch.begin()->second.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, families.data());

	physicalDevices[*pDevice] = physicalDevice;
	deviceInfos[*pDevice] = {props, memProperties, families};

	*logger << logger::begin << "Debug?" << std::boolalpha << fpGetDeviceProcAddr(*pDevice, "vkSetDebugUtilsObjectNameEXT") << logger::end;

	return ret;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_GetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties)
{
	*pQueueFamilyPropertyCount = 1;
	instance_dispatch.begin()->second.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
	*pQueueFamilyPropertyCount = 1;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_GetPhysicalDeviceQueueFamilyProperties2(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2*                   pQueueFamilyProperties)
{
	*pQueueFamilyPropertyCount = 1;
	instance_dispatch.begin()->second.GetPhysicalDeviceQueueFamilyProperties2(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
	*pQueueFamilyPropertyCount = 1;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_GetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueFamilyIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	*logger << logger::begin << "GetDeviceQueue: " << device << ", " << queueFamilyIndex << ", " << queueIndex << " -> " << *pQueue << logger::end;

	device_dispatch[GetKey(device)].GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);

	queueDevices[*pQueue] = device;

	if(!(deviceInfos[device].queueFamilies[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		return;

	if(!transferQueues.contains(device))
	{
		VkQueue layerQueue;
		device_dispatch[GetKey(device)].GetDeviceQueue(device, queueFamilyIndex, queueIndex, &layerQueue);
		*reinterpret_cast<void**>(layerQueue) = *reinterpret_cast<void**>(device);
		graphicsQueues[device] = layerQueue;
		transferQueues[device] = layerQueue;

		VkCommandPoolCreateInfo poolInfo;
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndex;
		VkCommandPool pool;
		if(device_dispatch[GetKey(device)].CreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
			*logger << logger::begin << logger::error << "failed to create command pool" << logger::end;
		transferPools[device] = pool;

		VkCommandBufferAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.commandPool = pool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer;
		if(device_dispatch[GetKey(device)].AllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS)
			*logger << logger::begin << logger::error << "failed to allocate command buffer" << logger::end;
		*reinterpret_cast<void**>(commandBuffer) = *reinterpret_cast<void**>(device);
		transferCommandBuffers[device] = commandBuffer;

		*logger << logger::begin << "Created transfer command buffer " << commandBuffer << " in pool " << pool << " for queue family " << queueFamilyIndex << " on device " << device << logger::end;

		// now we are "ready"
		CheekyLayer::active_logger log = *logger << logger::begin;
		CheekyLayer::rules::local_context ctx = { .logger = log, .device = device };
		CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::DeviceCreate, (VkHandle) device, ctx);
		log << logger::end;
	}
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	scoped_lock l(global_lock);
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

	device_dispatch.erase(GetKey(device));
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
