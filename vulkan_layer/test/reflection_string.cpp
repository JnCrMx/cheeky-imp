#include <assert.h>
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "reflection/reflectionparser.hpp"

int main()
{
    VkGraphicsPipelineCreateInfo info;
	VkPipelineDepthStencilStateCreateInfo depth;

    depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depth.depthTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_TRUE;
	info.pDepthStencilState = &depth;
    info.stageCount = 12;

    std::string a = CheekyLayer::reflection::parse_get_string("pDepthStencilState->depthCompareOp", &info, "VkGraphicsPipelineCreateInfo");
    assert(a == "VK_COMPARE_OP_ALWAYS");

    std::string b = CheekyLayer::reflection::parse_get_string("pDepthStencilState->depthTestEnable", &info, "VkGraphicsPipelineCreateInfo");
    assert(b == "VK_FALSE");

    std::string c = CheekyLayer::reflection::parse_get_string("pDepthStencilState->stencilTestEnable", &info, "VkGraphicsPipelineCreateInfo");
    assert(c == "VK_TRUE");

    std::string d = CheekyLayer::reflection::parse_get_string("stageCount", &info, "VkGraphicsPipelineCreateInfo");
    assert(d == "12");

	return 0;
}
