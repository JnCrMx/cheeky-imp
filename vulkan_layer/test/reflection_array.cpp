#include <assert.h>
#include <cstdint>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "reflection/reflectionparser.hpp"

int main()
{
	VkGraphicsPipelineCreateInfo info;
	VkPipelineViewportStateCreateInfo viewport;
    VkRect2D scissors[2] = {{.extent = {.width = 1024}}, {.extent = {.width = 1023}}};

	viewport.scissorCount = 2;
    viewport.pScissors = scissors;

	info.pViewportState = &viewport;

    {   // test pViewportState->pScissors[0]
	    std::any a = CheekyLayer::reflection::parse_get("pViewportState->pScissors[0].extent.width", &info, "VkGraphicsPipelineCreateInfo");
	    assert(std::any_cast<uint32_t>(a) == 1024);

        CheekyLayer::reflection::parse_set("pViewportState->pScissors[0].extent.width", &info, "VkGraphicsPipelineCreateInfo", 512u);

	    std::any b = CheekyLayer::reflection::parse_get("pViewportState->pScissors[0].extent.width", &info, "VkGraphicsPipelineCreateInfo");
	    assert(std::any_cast<uint32_t>(b) == 512);

	    CheekyLayer::reflection::parse_assign("pViewportState->pScissors[0].extent.width = 128", &info, "VkGraphicsPipelineCreateInfo");

	    std::any c = CheekyLayer::reflection::parse_get("pViewportState->pScissors[0].extent.width", &info, "VkGraphicsPipelineCreateInfo");
	    assert(std::any_cast<uint32_t>(c) == 128);
    }

    {   // test pViewportState->pScissors[1]
        std::any a = CheekyLayer::reflection::parse_get("pViewportState->pScissors[1].extent.width", &info, "VkGraphicsPipelineCreateInfo");
	    assert(std::any_cast<uint32_t>(a) == 1023);

        CheekyLayer::reflection::parse_set("pViewportState->pScissors[1].extent.width", &info, "VkGraphicsPipelineCreateInfo", 511u);

	    std::any b = CheekyLayer::reflection::parse_get("pViewportState->pScissors[1].extent.width", &info, "VkGraphicsPipelineCreateInfo");
	    assert(std::any_cast<uint32_t>(b) == 511);

	    CheekyLayer::reflection::parse_assign("pViewportState->pScissors[1].extent.width = 127", &info, "VkGraphicsPipelineCreateInfo");

	    std::any c = CheekyLayer::reflection::parse_get("pViewportState->pScissors[1].extent.width", &info, "VkGraphicsPipelineCreateInfo");
	    assert(std::any_cast<uint32_t>(c) == 127);
    }

    {   // test if changing pViewportState->pScissors[1] does not effect pViewportState->pScissors[0]
        std::any a = CheekyLayer::reflection::parse_get("pViewportState->pScissors[0].extent.width", &info, "VkGraphicsPipelineCreateInfo");
	    assert(std::any_cast<uint32_t>(a) == 128);
    }

    try // test range check
    {
	    std::any a = CheekyLayer::reflection::parse_get("pViewportState->pScissors[2].extent.width", &info, "VkGraphicsPipelineCreateInfo");
	    assert(false);
    }
    catch(const std::runtime_error& e)
    {
        assert(std::string(e.what()) == "array index 2 for member \"pScissors\" exceeds its length of 2 which can be found in member \"scissorCount\"");
    }

	return 0;
}
