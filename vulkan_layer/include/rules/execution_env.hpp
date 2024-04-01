#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <spdlog/logger.h>
#include <sys/socket.h>
#include <vulkan/vulkan.h>
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

namespace CheekyLayer {
	struct instance;
	struct device;
	struct command_buffer_state;
}

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

	enum data_type : int;
	struct user_function
	{
		class data* data;
		std::vector<data_type> arguments;
		std::vector<class data*> default_arguments;
	};

	class global_context
	{
		public:
			std::map<VkHandle, std::set<std::string>> marks;
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

			std::unordered_map<std::string, std::unique_ptr<ipc::file_descriptor>> fds;
			std::vector<std::thread> threads;

			std::unordered_map<std::string, data_value> global_variables;
			std::unordered_map<std::string, user_function> user_functions;
	};

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
		ipc::file_descriptor* socket;
		uint8_t* buffer;
		size_t size;
		int extra = 0;
	};

	struct present_info
	{
		const VkPresentInfoKHR* info;
	};

	struct swapchain_info
	{
		const VkSwapchainCreateInfoKHR* info;
	};

	using additional_info = std::variant<
		draw_info,
		pipeline_info,
		receive_info,
		present_info,
		swapchain_info
	>;

	struct calling_context
	{
		std::function<void(spdlog::logger&)> printVerbose;
		std::optional<additional_info> info;

		VkCommandBuffer commandBuffer;
		command_buffer_state* commandBufferState;

		bool canceled;
		std::vector<std::string> overrides;
		std::string customTag;
		std::vector<std::function<void(VkHandle)>> creationCallbacks;

		std::unordered_map<std::string, data_value> local_variables;

		void* customPointer;
	};

	struct local_context
	{
		spdlog::logger& logger;
		std::function<void(spdlog::logger&)> printVerbose;

		additional_info* info;

		CheekyLayer::instance* instance;
		CheekyLayer::device* device;

		VkCommandBuffer commandBuffer;
		command_buffer_state* commandBufferState;

		bool& canceled;
		std::vector<std::string>& overrides;
		const std::string& customTag;
		std::vector<std::function<void(VkHandle)>>& creationCallbacks;

		int currentIndex;
		data_value* currentElement;
		data_value* currentReduction;

		std::unordered_map<std::string, data_value>& local_variables;

		void*& customPointer;
	};
}
