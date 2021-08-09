#pragma once

#include "layer.hpp"
#include "rules/execution_env.hpp"
#include <vulkan/vulkan_core.h>

using CheekyLayer::VkHandle;

struct ShaderInfo
{
	VkShaderStageFlagBits stage;
	VkShaderModule module;
	VkHandle customHandle;
	std::string hash;
	std::string name;
};

struct PipelineState
{
	std::vector<ShaderInfo> stages;
	std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
};

inline VkLayerDispatchTable quick_dispatch;

struct CommandBufferState
{
	VkDevice device;
	VkPipeline pipeline;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<uint32_t> descriptorDynamicOffsets;

	std::vector<VkBuffer> vertexBuffers;
	std::vector<VkDeviceSize> vertexBufferOffsets;
	
	VkBuffer indexBuffer;
	VkDeviceSize indexBufferOffset;
	VkIndexType indexType;

	std::vector<VkRect2D> scissors;

	VkRenderPass renderpass;
	VkFramebuffer framebuffer;
};

extern std::map<VkPipeline, PipelineState> pipelineStates;
extern std::map<VkCommandBuffer, CommandBufferState> commandBufferStates;
inline bool evalRulesInDraw;
