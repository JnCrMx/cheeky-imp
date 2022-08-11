#pragma once

#include "layer.hpp"
#include "rules/execution_env.hpp"
#include <vulkan/vulkan_core.h>

using CheekyLayer::rules::VkHandle;

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


struct BufferBinding
{
	VkBuffer buffer;
	VkDeviceSize offset;
	VkDeviceSize size;
};

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

	bool transformFeedback;
};

struct FramebufferInfo
{
	std::vector<VkImageView> attachments;
	uint32_t width;
	uint32_t height;

	FramebufferInfo() {}

	FramebufferInfo(const VkFramebufferCreateInfo* i)
	{
		attachments = std::vector<VkImageView>(i->pAttachments, i->pAttachments + i->attachmentCount);
		width = i->width;
		height = i->height;
	}
};

struct PipelineLayoutInfo
{
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::vector<VkPushConstantRange> pushConstantRanges;
};

extern std::map<VkPipelineLayout, PipelineLayoutInfo> pipelineLayouts;
extern std::map<VkFramebuffer, FramebufferInfo> framebuffers;
extern std::map<VkPipeline, PipelineState> pipelineStates;
extern std::map<VkCommandBuffer, CommandBufferState> commandBufferStates;
inline bool evalRulesInDraw;
