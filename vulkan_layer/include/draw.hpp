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
};

extern std::map<VkPipeline, PipelineState> pipelineStates;
extern std::map<VkCommandBuffer, CommandBufferState> commandBufferStates;
