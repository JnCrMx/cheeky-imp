#include <assert.h>
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "reflection/reflectionparser.hpp"
#include "reflection/custom_structs.hpp"

int main()
{
	CheekyLayer::reflection::VkCmdDrawIndexed draw = {
        .indexCount = 32,
        .firstIndex = 1337,
        .vertexOffset = 753
    };

	std::any a = CheekyLayer::reflection::parse_get("indexCount", &draw, "VkCmdDrawIndexed");
	assert(std::any_cast<uint32_t>(a) == 32);

	std::any b = CheekyLayer::reflection::parse_get("firstIndex", &draw, "VkCmdDrawIndexed");
	assert(std::any_cast<uint32_t>(b) == 1337);

	std::any c = CheekyLayer::reflection::parse_get("vertexOffset", &draw, "VkCmdDrawIndexed");
	assert(std::any_cast<int32_t>(c) == 753);

	return 0;
}
