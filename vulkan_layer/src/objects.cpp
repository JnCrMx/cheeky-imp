#include "objects.hpp"
#include "constants.hpp"
#include "rules/reader.hpp"
#include "utils.hpp"

#include <filesystem>
#include <fmt/core.h>
#include <spdlog/async.h>
#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <sstream>
#include <string>
#include <vulkan/vulkan_core.h>

namespace CheekyLayer {

unsigned int instance::instance_id = 0;

instance::instance(const VkInstanceCreateInfo *pCreateInfo, VkInstance* pInstance)
 : id(instance_id++), handle(*pInstance) {
    std::fill(reinterpret_cast<char*>(&dispatch), reinterpret_cast<char*>(&dispatch)+sizeof(dispatch), 0);

    try
	{
		std::string configFile = std::getenv(CheekyLayer::Contants::CONFIG_ENV.c_str());
		std::ifstream in(configFile);
		if(in.good())
		{
			in >> config;
		}
	}
	catch(std::exception&) {}

    std::string applicationName = "unknown";
	std::string engineName = "unknown";
	if(pCreateInfo->pApplicationInfo && pCreateInfo->pApplicationInfo->pApplicationName)
		applicationName = pCreateInfo->pApplicationInfo->pApplicationName;
	if(pCreateInfo->pApplicationInfo && pCreateInfo->pApplicationInfo->pEngineName)
		engineName = pCreateInfo->pApplicationInfo->pEngineName;

    enabled = config.application.empty() || config.application == applicationName;

    if(!enabled) {
        return;
    }
    hook_draw_calls = config.hook_draw_calls;

	std::string logfile = config.log_file;
	replace(logfile, "{{pid}}", std::to_string(getpid()));
	replace(logfile, "{{inst}}", std::to_string((uint64_t)*pInstance));

    logger = spdlog::basic_logger_mt<spdlog::async_factory>(fmt::format("instance #{}: {}", id, fmt::ptr(handle)), logfile);
    logger->flush_on(spdlog::level::warn);

    logger->info("Hello from {} version {} from {} {} for application {} using engine {}",
		CheekyLayer::Contants::LAYER_NAME, CheekyLayer::Contants::GIT_VERSION,
		__DATE__, __TIME__, applicationName, engineName);
    logger->flush();

    for(std::string type : {"images", "buffers", "shaders"})
	{
		try
		{
			std::filesystem::directory_iterator it(config.override_directory / type);

			int count = 0;
			std::transform(std::filesystem::begin(it), std::filesystem::end(it), std::inserter(overrideCache, overrideCache.end()), [&count](auto e){
				count++;
				return e.path().stem();
			});
            logger->info("Found {} overrides for {}", count, type);
		}
		catch (std::filesystem::filesystem_error& ex)
		{
            logger->warn("Cannot find overrides for {}: {}", type, ex.what());
		}
	}

    std::ifstream rulesIn(config.rule_file);
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
            logger->error("Error at {}:{}: {}", numberer.line(), numberer.col(), ex.what());
            break;
        }
    }

    logger->info("Loaded {} rules:", rules.size());
	for(auto& r : rules)
	{
		std::ostringstream oss;
        r->print(oss);
        logger->info("\n{}", oss.str());
	}
    logger->flush();
}

instance& instance::operator=(instance&& other) {
    id = other.id;
    handle = other.handle;
    dispatch = std::move(other.dispatch);
    config = std::move(other.config);
    enabled = other.enabled;
    hook_draw_calls = other.hook_draw_calls;
    overrideCache = std::move(other.overrideCache);
    dumpCache = std::move(other.dumpCache);
    logger = std::move(other.logger);
    rules = std::move(other.rules);
    has_rules = std::move(other.has_rules);
    devices = std::move(other.devices);
    return *this;
}

device::device(instance* inst, PFN_vkGetDeviceProcAddr gdpa, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, VkDevice *pDevice)
    : inst(inst), handle(*pDevice), gdpa(gdpa), physicalDevice(physicalDevice) {
    logger = inst->logger->clone(fmt::format("device #{}/{}: {}", inst->id, inst->devices.size(), fmt::ptr(handle)));
    logger->info("Created device {}", fmt::ptr(handle));
    std::fill(reinterpret_cast<char*>(&dispatch), reinterpret_cast<char*>(&dispatch)+sizeof(dispatch), 0);

    inst->dispatch.GetPhysicalDeviceProperties(physicalDevice, &props);
    inst->dispatch.GetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
	uint32_t count;
	inst->dispatch.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
    queueFamilies.resize(count);
    inst->dispatch.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queueFamilies.data());

    has_debug = gdpa(*pDevice, "vkSetDebugUtilsObjectNameEXT");
    logger->info(has_debug ? "Device has debug utils" : "Device does not have debug utils");
}

}
