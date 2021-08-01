#pragma once

#include "layer.hpp"
#include <vulkan/vulkan_core.h>

struct ShaderInfo
{
	VkShaderStageFlagBits stage;
	VkShaderModule module;
	std::string hash;
	std::string name;
};

struct PipelineState
{
	std::vector<ShaderInfo> stages;
	std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
};

struct QuickDispatch
{
	bool filled;
	PFN_vkCmdDrawIndexed CmdDrawIndexed;
	PFN_vkCmdDraw CmdDraw;
	PFN_vkCmdBindPipeline CmdBindPipeline;
	PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
	PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
	PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
	PFN_vkCmdBindVertexBuffers2EXT CmdBindVertexBuffers2EXT;
	PFN_vkCmdSetScissor CmdSetScissor;
	PFN_vkEndCommandBuffer EndCommandBuffer;
};
inline QuickDispatch quick_dispatch = {false};

struct CommandBufferState
{
	VkDevice device;
	VkPipeline pipeline;
	std::vector<VkDescriptorSet> descriptorSets;

	std::vector<VkBuffer> vertexBuffers;
	std::vector<VkDeviceSize> vertexBufferOffsets;
	
	VkBuffer indexBuffer;
	VkDeviceSize indexBufferOffset;
	VkIndexType indexType;

	std::vector<VkRect2D> scissors;
};

extern std::map<VkPipeline, PipelineState> pipelineStates;
extern std::map<VkCommandBuffer, CommandBufferState> commandBufferStates;
inline bool evalRulesInDraw;
