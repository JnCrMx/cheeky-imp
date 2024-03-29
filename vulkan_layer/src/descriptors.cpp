#include "layer.hpp"
#include "dispatch.hpp"
#include "descriptors.hpp"
#include "images.hpp"
#include "draw.hpp"
#include "rules/rules.hpp"
#include <vulkan/vulkan_core.h>

#include <stdexcept>

using CheekyLayer::logger;

std::map<VkDescriptorUpdateTemplate, std::vector<VkDescriptorUpdateTemplateEntry>> updateTemplates;
std::map<VkDescriptorSet, DescriptorState> descriptorStates;

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateDescriptorUpdateTemplate(
	VkDevice                                    device,
	const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkDescriptorUpdateTemplate*                 pDescriptorUpdateTemplate)
{
	VkResult r = device_dispatch[GetKey(device)].CreateDescriptorUpdateTemplate(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);

	*logger << logger::begin << "CreateDescriptorUpdateTemplate: " << pCreateInfo->descriptorUpdateEntryCount << " -> " << *pDescriptorUpdateTemplate << logger::end;
	updateTemplates[*pDescriptorUpdateTemplate] =
		std::vector(pCreateInfo->pDescriptorUpdateEntries, pCreateInfo->pDescriptorUpdateEntries+pCreateInfo->descriptorUpdateEntryCount);

	return r;
}

bool from_descriptorType(VkDescriptorType type, CheekyLayer::rules::selector_type& outType)
{
	switch(type)
	{
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			outType = CheekyLayer::rules::selector_type::Image;
			return true;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			outType = CheekyLayer::rules::selector_type::Buffer;
			return true;
		default:
			return false;
	}
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_UpdateDescriptorSetWithTemplate(
	VkDevice                                    device,
	VkDescriptorSet                             descriptorSet,
	VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
	const void*                                 pData)
{
	device_dispatch[GetKey(device)].UpdateDescriptorSetWithTemplate(device, descriptorSet, descriptorUpdateTemplate, pData);
	if(!evalRulesInDraw)
		return;

	DescriptorState& state = descriptorStates[descriptorSet];

	auto& info = updateTemplates[descriptorUpdateTemplate];
	for(int i=0; i<info.size(); i++)
	{
		VkDescriptorUpdateTemplateEntry& entry = info[i];
		
		DescriptorBinding& binding = state.bindings[entry.dstBinding];
		try
		{
			CheekyLayer::rules::selector_type type;
			if(!from_descriptorType(entry.descriptorType, type))
				continue;
			binding.type = type;
			binding.exactType = entry.descriptorType;

			if(binding.arrayElements.size() < entry.dstArrayElement + entry.descriptorCount)
				binding.arrayElements.resize(entry.dstArrayElement + entry.descriptorCount);
			
			for(int j=0; j<entry.descriptorCount; j++)
			{
				const void* newPtr = (static_cast<const uint8_t*>(pData)) + entry.offset + j*entry.stride;
				if(type == CheekyLayer::rules::selector_type::Image)
				{
					const VkDescriptorImageInfo* info = (const VkDescriptorImageInfo*) newPtr;

					binding.arrayElements[entry.dstArrayElement + j] = {imageViews[info->imageView], *info};
				}
				else if(type == CheekyLayer::rules::selector_type::Buffer)
				{
					const VkDescriptorBufferInfo* info = (const VkDescriptorBufferInfo*) newPtr;

					binding.arrayElements[entry.dstArrayElement + j] = {info->buffer, *info};
				}
			}
		}
		catch(std::exception& ex)
		{
			*logger << logger::begin << logger::error << "UpdateDescriptorSetWithTemplate: " << ex.what() << logger::end;
		}
	}
}
