#include "layer.hpp"
#include "objects.hpp"
#include "rules/rules.hpp"
#include <vulkan/vulkan_core.h>

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

namespace CheekyLayer {

VkResult device::CreateDescriptorUpdateTemplate(const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate)
{
	VkResult result = dispatch.CreateDescriptorUpdateTemplate(handle, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
	if(result != VK_SUCCESS)
		return result;

	logger->debug("CreateDescriptorUpdateTemplate: {} -> {}", pCreateInfo->descriptorUpdateEntryCount, fmt::ptr(pDescriptorUpdateTemplate));
	updateTemplates[*pDescriptorUpdateTemplate] =
		std::vector(pCreateInfo->pDescriptorUpdateEntries, pCreateInfo->pDescriptorUpdateEntries+pCreateInfo->descriptorUpdateEntryCount);

	return result;
}

void device::UpdateDescriptorSetWithTemplate(VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData)
{
	dispatch.UpdateDescriptorSetWithTemplate(handle, descriptorSet, descriptorUpdateTemplate, pData);

	descriptor_state& state = descriptorStates[descriptorSet];
	auto& info = updateTemplates[descriptorUpdateTemplate];
	for(int i=0; i<info.size(); i++)
	{
		VkDescriptorUpdateTemplateEntry& entry = info[i];

		descriptor_binding& binding = state.bindings[entry.dstBinding];
		try {
			rules::selector_type type;
			if(!from_descriptorType(entry.descriptorType, type))
				continue;
			binding.type = type;
			binding.exactType = entry.descriptorType;

			if(binding.arrayElements.size() < entry.dstArrayElement + entry.descriptorCount)
				binding.arrayElements.resize(entry.dstArrayElement + entry.descriptorCount);

			for(int j=0; j<entry.descriptorCount; j++) {
				const void* newPtr = (static_cast<const uint8_t*>(pData)) + entry.offset + j*entry.stride;
				switch(type) {
					case rules::Buffer: {
						const VkDescriptorImageInfo* info = (const VkDescriptorImageInfo*) newPtr;
						binding.arrayElements[entry.dstArrayElement + j] = {imageViewToImage[info->imageView], *info};
						break;
					}
					case rules::Image: {
						const VkDescriptorBufferInfo* info = (const VkDescriptorBufferInfo*) newPtr;
						binding.arrayElements[entry.dstArrayElement + j] = {info->buffer, *info};
						break;
					}
					default:
						logger->warn("UpdateDescriptorSetWithTemplate: Unknown descriptor type {}", rules::to_string(type));
						break;
				}
			}
		} catch(const std::exception& ex) {
			logger->error("UpdateDescriptorSetWithTemplate: {}", ex.what());
		}
	}
}

}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateDescriptorUpdateTemplate(
	VkDevice                                    device,
	const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkDescriptorUpdateTemplate*                 pDescriptorUpdateTemplate)
{
	return CheekyLayer::get_device(device).CreateDescriptorUpdateTemplate(pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_UpdateDescriptorSetWithTemplate(
	VkDevice                                    device,
	VkDescriptorSet                             descriptorSet,
	VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
	const void*                                 pData)
{
	return CheekyLayer::get_device(device).UpdateDescriptorSetWithTemplate(descriptorSet, descriptorUpdateTemplate, pData);
}
