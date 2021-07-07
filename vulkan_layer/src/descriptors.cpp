#include "layer.hpp"
#include "dispatch.hpp"
#include "descriptors.hpp"
#include "images.hpp"
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

CheekyLayer::selector_type from_descriptorType(VkDescriptorType type)
{
	switch(type)
	{
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			return CheekyLayer::selector_type::Image;
		default:
			throw unsupported_descriptor_type_exception(type);
	}
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_UpdateDescriptorSetWithTemplate(
	VkDevice                                    device,
	VkDescriptorSet                             descriptorSet,
	VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
	const void*                                 pData)
{
	device_dispatch[GetKey(device)].UpdateDescriptorSetWithTemplate(device, descriptorSet, descriptorUpdateTemplate, pData);

	if(descriptorStates.find(descriptorSet) == descriptorStates.end())
		descriptorStates[descriptorSet] = {};
	DescriptorState& state = descriptorStates[descriptorSet];

	auto info = updateTemplates[descriptorUpdateTemplate];
	for(int i=0; i<info.size(); i++)
	{
		VkDescriptorUpdateTemplateEntry& entry = info[i];
		
		if(state.bindings.find(entry.dstBinding) == state.bindings.end())
			state.bindings[entry.dstBinding] = {};

		try
		{
			CheekyLayer::selector_type type = from_descriptorType(entry.descriptorType);
			state.bindings[entry.dstBinding].type = type;

			if(state.bindings[entry.dstBinding].arrayElements.size() < entry.dstArrayElement + entry.descriptorCount)
				state.bindings[entry.dstBinding].arrayElements.resize(entry.dstArrayElement + entry.descriptorCount);
			
			for(int j=0; j<entry.descriptorCount; j++)
			{
				const void* newPtr = ((uint8_t*)pData) + entry.offset + j*entry.stride;
				if(type == CheekyLayer::selector_type::Image)
				{
					const VkDescriptorImageInfo* info = (const VkDescriptorImageInfo*) newPtr;

					state.bindings[entry.dstBinding].arrayElements[entry.dstArrayElement + j] = imageViews[info->imageView];
				}
			}
		}
		catch(unsupported_descriptor_type_exception& ex)
		{

		}
		catch(std::exception& ex)
		{
			*logger << logger::begin << "UpdateDescriptorSetWithTemplate: error: " << ex.what() << logger::end;
		}
	}
}
