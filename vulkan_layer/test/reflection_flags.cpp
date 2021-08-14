#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "reflection/reflectionparser.hpp"
#include "reflection/vkreflection.hpp"

TEST(reflection, flags)
{
	VkDescriptorSetLayoutBinding binding{};
	
	CheekyLayer::reflection::parse_assign("stageFlags = VK_SHADER_STAGE_VERTEX_BIT", &binding, "VkDescriptorSetLayoutBinding");
	EXPECT_EQ(binding.stageFlags, VK_SHADER_STAGE_VERTEX_BIT);

	CheekyLayer::reflection::parse_assign("stageFlags = 0", &binding, "VkDescriptorSetLayoutBinding");
	EXPECT_EQ(binding.stageFlags, 0);
	
	CheekyLayer::reflection::parse_assign("stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT", &binding, "VkDescriptorSetLayoutBinding");
	EXPECT_EQ(binding.stageFlags, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}
