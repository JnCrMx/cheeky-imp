#include <assert.h>
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "reflection/reflectionparser.hpp"

int main()
{
	VkGraphicsPipelineCreateInfo info;
	VkPipelineDepthStencilStateCreateInfo depth;

    depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
	info.pDepthStencilState = &depth;

	std::any a = CheekyLayer::reflection::parse_get("pDepthStencilState->depthCompareOp", &info, "VkGraphicsPipelineCreateInfo");
	assert(std::any_cast<uint32_t>(a) == VK_COMPARE_OP_ALWAYS);

	CheekyLayer::reflection::parse_set("pDepthStencilState->depthCompareOp", &info, "VkGraphicsPipelineCreateInfo", (uint32_t)VK_COMPARE_OP_GREATER);

	std::any b = CheekyLayer::reflection::parse_get("pDepthStencilState->depthCompareOp", &info, "VkGraphicsPipelineCreateInfo");
	assert(std::any_cast<uint32_t>(b) == VK_COMPARE_OP_GREATER);

	CheekyLayer::reflection::parse_assign("pDepthStencilState->depthCompareOp = VK_COMPARE_OP_NEVER", &info, "VkGraphicsPipelineCreateInfo");

	std::any c = CheekyLayer::reflection::parse_get("pDepthStencilState->depthCompareOp", &info, "VkGraphicsPipelineCreateInfo");
	assert(std::any_cast<uint32_t>(c) == VK_COMPARE_OP_NEVER);

	return 0;
}
