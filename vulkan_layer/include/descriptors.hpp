#pragma once

#include <map>
#include <variant>
#include <vulkan/vulkan_core.h>

#include "layer.hpp"
#include "rules/rules.hpp"
#include "rules/execution_env.hpp"

using CheekyLayer::rules::VkHandle;

extern std::map<VkDescriptorUpdateTemplate, std::vector<VkDescriptorUpdateTemplateEntry>> updateTemplates;

using DescriptorElementInfo = std::variant<VkDescriptorBufferInfo, VkDescriptorImageInfo>;

struct DescriptorElement
{
	VkHandle handle;
	DescriptorElementInfo info;
};

struct DescriptorBinding
{
	CheekyLayer::rules::selector_type type;
	VkDescriptorType exactType;
	std::vector<DescriptorElement> arrayElements;
};

struct DescriptorState
{
	std::map<int, DescriptorBinding> bindings;
};

extern std::map<VkDescriptorSet, DescriptorState> descriptorStates;

class unsupported_descriptor_type_exception : public std::runtime_error
{
	public:
		unsupported_descriptor_type_exception(VkDescriptorType type) :
			std::runtime_error("unsupported descriptor type "+std::to_string(type))
		{

		}
};
