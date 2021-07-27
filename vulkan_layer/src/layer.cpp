#include <algorithm>
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

#include "config.hpp"
#include "dispatch.hpp"
#include "constants.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "draw.hpp"

using CheekyLayer::logger;

std::mutex global_lock;
std::mutex transfer_lock;
CheekyLayer::config global_config;
CheekyLayer::logger* logger;
std::vector<std::string> overrideCache;

std::vector<VkInstance> instances;
std::vector<std::unique_ptr<CheekyLayer::rule>> rules;
std::map<CheekyLayer::selector_type, bool> has_rules;

std::map<VkDevice, VkQueue> transferQueues;
std::map<VkDevice, VkCommandPool> transferPools;
std::map<VkDevice, VkCommandBuffer> transferCommandBuffers;
std::map<VkDevice, VkDeviceInfo> deviceInfos;

void update_has_rules()
{
	has_rules[CheekyLayer::selector_type::Buffer] = false;
	has_rules[CheekyLayer::selector_type::Draw] = false;
	has_rules[CheekyLayer::selector_type::Image] = false;
	has_rules[CheekyLayer::selector_type::Shader] = false;

	for(auto& r : rules)
	{
		if(r->is_enabled())
			has_rules[r->get_type()] = true;
	}

	evalRulesInDraw = has_rules[CheekyLayer::selector_type::Draw];
}

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
	instances.push_back(*pInstance);

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

	std::string applicationName = "unknown";
	std::string engineName = "unknown";
	if(pCreateInfo->pApplicationInfo && pCreateInfo->pApplicationInfo->pApplicationName)
		applicationName = pCreateInfo->pApplicationInfo->pApplicationName;
	if(pCreateInfo->pApplicationInfo && pCreateInfo->pApplicationInfo->pEngineName)
		engineName = pCreateInfo->pApplicationInfo->pEngineName;

	std::string logfile = global_config["logFile"];
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
		<< " for application " << applicationName
		<< " using engine " << engineName << logger::end;
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
			*logger << logger::begin << logger::error << "Cannot find overrides for " << type << ": " << ex.what() << logger::end;
		}
	}

	std::string rulefile = global_config["ruleFile"];
	std::ifstream rulesIn(rulefile);
	rules.clear();
	if(rulesIn.good())
	{
		std::string line;
		int lineNr = 0;
		while(std::getline(rulesIn, line))
		{
			lineNr++;
			if(!line.starts_with("#") && !line.empty())
			{
				try
				{
					std::unique_ptr<CheekyLayer::rule> rule = std::make_unique<CheekyLayer::rule>();
					std::istringstream iss(line);
					iss >> *rule;

					rules.push_back(std::move(rule));
				}
				catch(const std::exception& ex)
				{
					*logger << logger::begin << logger::error << "Error in rulefile " << rulefile << " line " << lineNr << ": " << ex.what() << logger::end;
				}
			}
		}
	}
	*logger << logger::begin << "Loaded " << rules.size() << " rules:" << logger::end;
	for(auto& r : rules)
	{
		CheekyLayer::active_logger log = *logger << logger::begin;
		log << "    ";
		r->print(log.raw());
		log << logger::end;
	}
	CheekyLayer::rule_disable_callback = [](CheekyLayer::rule* rule){
		update_has_rules();
	};
	update_has_rules();

	// close all file descriptors when creating a new instance
	for(auto& [name, fd] : CheekyLayer::rule_env.fds)
	{
		fd->close();
	}
	CheekyLayer::rule_env.fds.clear();

	if(!layer_disabled)
	{
		CheekyLayer::active_logger log = *logger << logger::begin;
		CheekyLayer::local_context ctx = { .logger = log };
		CheekyLayer::execute_rules(rules, CheekyLayer::selector_type::Init, VK_NULL_HANDLE, ctx);
		log << logger::end;
	}

	return ret;
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
	if(instance_dispatch.find(GetKey(instance)) != instance_dispatch.end())
		instance_dispatch.erase(GetKey(instance));

	*logger << logger::begin << "DestroyInstance: " << instance << logger::end;

	for(auto& [name, fd] : CheekyLayer::rule_env.fds)
	{
		fd->close();
	}
	CheekyLayer::rule_env.fds.clear();
	CheekyLayer::rule_env.threads.clear();
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

	VkPhysicalDeviceMemoryProperties memProperties;
	instance_dispatch.begin()->second.GetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	uint32_t count;
	instance_dispatch.begin()->second.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
	std::vector<VkQueueFamilyProperties> families(count);
	instance_dispatch.begin()->second.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, families.data());

	deviceInfos[*pDevice] = {props, memProperties, families};

	return ret;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_GetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueFamilyIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	*logger << logger::begin << "GetDeviceQueue: " << device << ", " << queueFamilyIndex << ", " << queueIndex << " -> " << *pQueue << logger::end;

	device_dispatch[GetKey(device)].GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);

	if(!(deviceInfos[device].queueFamilies[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		return;

	if(!transferQueues.count(device))
	{
		transferQueues[device] = *pQueue;

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
		VkCommandBuffer buffer;
		if(device_dispatch[GetKey(device)].AllocateCommandBuffers(device, &allocateInfo, &buffer) != VK_SUCCESS)
			*logger << logger::begin << logger::error << "failed to allocate command buffer" << logger::end;
		transferCommandBuffers[device] = buffer;

		*logger << logger::begin << "Created transfer command buffer " << buffer << " in pool " << pool << " for queue family " << queueFamilyIndex << " on device " << device << logger::end;
	}
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_DestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
	device_dispatch[GetKey(device)].DestroyCommandPool(device, transferPools[device], nullptr);
	transferCommandBuffers.erase(device);
	transferPools.erase(device);
	transferQueues.erase(device);

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
