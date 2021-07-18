#include <assert.h>
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "reflection/reflectionparser.hpp"

int main()
{
	VkGraphicsPipelineCreateInfo info;
	VkPipelineDepthStencilStateCreateInfo depth;

	depth.depthTestEnable = VK_TRUE;
	info.pDepthStencilState = &depth;

	std::any a = CheekyLayer::reflection::parse_get("pDepthStencilState->depthTestEnable", &info, "VkGraphicsPipelineCreateInfo");
	assert(std::any_cast<VkBool32>(a) == VK_TRUE);

	CheekyLayer::reflection::parse_set("pDepthStencilState->depthTestEnable", &info, "VkGraphicsPipelineCreateInfo", VK_FALSE);

	std::any b = CheekyLayer::reflection::parse_get("pDepthStencilState->depthTestEnable", &info, "VkGraphicsPipelineCreateInfo");
	assert(std::any_cast<VkBool32>(b) == VK_FALSE);

	CheekyLayer::reflection::parse_assign("pDepthStencilState->depthTestEnable = VK_TRUE", &info, "VkGraphicsPipelineCreateInfo");

	std::any c = CheekyLayer::reflection::parse_get("pDepthStencilState->depthTestEnable", &info, "VkGraphicsPipelineCreateInfo");
	assert(std::any_cast<VkBool32>(c) == VK_TRUE);

	return 0;
}
