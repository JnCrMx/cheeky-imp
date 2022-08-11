#pragma once

#include <cstdint>
#include <memory>
#include <sys/socket.h>
#include <vulkan/vulkan.h>
#include "logger.hpp"
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <thread>
#include <variant>
#include <vulkan/vulkan_core.h>
#include "rules/ipc.hpp"
#include "reflection/custom_structs.hpp"

struct CommandBufferState;
namespace CheekyLayer::rules
{
	#if (VK_USE_64_BIT_PTR_DEFINES==1)
		typedef void* VkHandle;
	#else
		typedef uint64_t VkHandle;
	#endif

	enum selector_type : int;
	struct local_context;

	struct data_list;
	using data_value = std::variant<std::string, std::vector<uint8_t>, VkHandle, double, data_list>;

	struct stored_handle
	{
		VkHandle handle;
		selector_type type;
		VkDevice device;
	};

	class global_context
	{
		public:
			std::map<VkHandle, std::vector<std::string>> marks;
			std::map<VkHandle, std::string> hashes;

			std::map<VkCommandBuffer, std::vector<std::function<void(local_context&)>>> on_EndCommandBuffer;
			std::map<VkCommandBuffer, std::vector<std::function<void(local_context&)>>> on_QueueSubmit;
			std::map<VkCommandBuffer, std::vector<std::function<void(local_context&)>>> on_EndRenderPass;

			void onEndCommandBuffer(VkCommandBuffer commandBuffer, std::function<void(local_context&)> function)
			{
				on_EndCommandBuffer[commandBuffer].push_back(function);
			}
			void onQueueSubmit(VkCommandBuffer commandBuffer, std::function<void(local_context&)> function)
			{
				on_QueueSubmit[commandBuffer].push_back(function);
			}
			void onEndRenderPass(VkCommandBuffer commandBuffer, std::function<void(local_context&)> function)
			{
				on_EndRenderPass[commandBuffer].push_back(function);
			}

			std::map<std::string, std::unique_ptr<ipc::file_descriptor>> fds;
			std::map<std::string, stored_handle> handles;

			std::vector<std::thread> threads;

			std::map<std::string, data_value> global_variables;
	};

	inline global_context rule_env;

	struct draw_info
	{
		std::vector<VkImage>& images;
		std::vector<VkHandle>& shaders;
		std::vector<VkBuffer>& vertexBuffers;
		VkBuffer indexBuffer;

		std::variant<const reflection::VkCmdDrawIndexed*, const reflection::VkCmdDraw*> info;
	};

	struct pipeline_info
	{
		std::vector<VkHandle>& shaderStages;
		const VkGraphicsPipelineCreateInfo* info;
	};

	struct receive_info
	{
		ipc::socket* socket;
		uint8_t* buffer;
		size_t size;
	};

	struct present_info
	{
		const VkPresentInfoKHR* info;
	};

	struct swapchain_info
	{
		const VkSwapchainCreateInfoKHR* info;
	};

	union additional_info
	{
		draw_info draw;
		pipeline_info pipeline;
		receive_info receive;
		present_info present;
		swapchain_info swapchain;
	};

	struct local_context
	{
		active_logger& logger;
		std::optional<std::function<void(active_logger)>> printVerbose;
		additional_info* info;
		VkCommandBuffer commandBuffer;
		VkDevice device;
		bool canceled = false;
		std::vector<std::string> overrides;
		CommandBufferState* commandBufferState;
		std::string customTag;
		std::vector<std::function<void(VkHandle)>> creationConsumers;

		int currentIndex;
		data_value* currentElement;
		data_value* currentReduction;

		std::map<std::string, data_value> local_variables;
	};
}
