#pragma once

#include <vulkan/vulkan.h>
#include "logger.hpp"
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <vulkan/vulkan_core.h>

namespace CheekyLayer
{
	#if (VK_USE_64_BIT_PTR_DEFINES==1)
		typedef void* VkHandle;
	#else
		typedef uint64_t VkHandle;
	#endif

	enum selector_type : int;
	struct local_context;

	class global_context
	{
		public:
			std::map<VkHandle, std::vector<std::string>> marks;
			std::map<VkHandle, std::string> hashes;

			std::map<VkCommandBuffer, std::vector<std::function<void(local_context&)>>> on_EndCommandBuffer;
			std::map<VkCommandBuffer, std::vector<std::function<void(local_context&)>>> on_QueueSubmit;

			void onEndCommandBuffer(VkCommandBuffer commandBuffer, std::function<void(local_context&)> function)
			{
				on_EndCommandBuffer[commandBuffer].push_back(function);
			}
			void onQueueSubmit(VkCommandBuffer commandBuffer, std::function<void(local_context&)> function)
			{
				on_QueueSubmit[commandBuffer].push_back(function);
			}
	};

	inline global_context rule_env;

	struct draw_info
	{
		std::vector<VkImage>& images;
		std::vector<VkShaderModule>& shaders;
		std::vector<VkBuffer>& vertexBuffers;
		VkBuffer indexBuffer;
	};

	struct pipeline_info
	{
		std::vector<VkShaderModule>& shaderStages;
		const VkGraphicsPipelineCreateInfo* info;
	};

	union additional_info
	{
		draw_info draw;
		pipeline_info pipeline;
	};

	struct local_context
	{
		active_logger& logger;
		std::optional<std::function<void(active_logger)>> printVerbose;
		additional_info* info;
		VkCommandBuffer commandBuffer;
		bool canceled = false;
		std::vector<std::string> overrides;
	};
}
