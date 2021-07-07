#include "layer.hpp"
#include "dispatch.hpp"
#include "buffers.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "utils.hpp"
#include "images.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vk_format_utils.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vulkan/vulkan_core.h>

#ifdef USE_IMAGE_TOOLS
#include <block_compression.hpp>
#include <image.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

#if defined(USE_IMAGE_TOOLS) && defined(EXPORT_PNG)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#endif

using CheekyLayer::logger;
using CheekyLayer::VkHandle;

std::map<VkImage, VkImageCreateInfo> images;
std::map<VkImageView, VkImage> imageViews;

#ifdef USE_IMAGE_TOOLS
std::map<VkImage, std::unique_ptr<image_tools::image>> topResolutions;
#endif

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateImage(
	VkDevice                                    device,
	const VkImageCreateInfo*                    pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkImage*                                    pImage)
{
	VkResult ret = device_dispatch[GetKey(device)].CreateImage(device, pCreateInfo, pAllocator, pImage);
	images[*pImage] = *pCreateInfo;

	VkMemoryRequirements memRequirements;
	device_dispatch[GetKey(device)].GetImageMemoryRequirements(device, *pImage, &memRequirements);

	*logger << logger::begin << "CreateImage: " << pCreateInfo->extent.width << "x" << pCreateInfo->extent.height
			<< " @ " << pCreateInfo->mipLevels << " samples=" << pCreateInfo->samples
			<< " memory=" << memRequirements.size << " image=" << *pImage << logger::end;

	return ret;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_BindImageMemory(
	VkDevice                                    device,
	VkImage                                     image,
	VkDeviceMemory                              memory,
	VkDeviceSize                                memoryOffset)
{
	*logger << logger::begin << "BindImageMemory: image=" << image << std::hex << " memory=" << memory << " offset=" << memoryOffset << logger::end;

	return device_dispatch[GetKey(device)].BindImageMemory(device, image, memory, memoryOffset);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateImageView(
	VkDevice                                    device,
	const VkImageViewCreateInfo*                pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkImageView*                                pView)
{
	VkResult result = device_dispatch[GetKey(device)].CreateImageView(device, pCreateInfo, pAllocator, pView);

	imageViews[*pView] = pCreateInfo->image;

	return result;
}

static inline bool IsExtentSizeZero(const VkExtent3D* extent) {
	return ((extent->width == 0) || (extent->height == 0) || (extent->depth == 0));
}

static inline VkDeviceSize GetBufferSizeFromCopyImage(VkBufferImageCopy region, VkFormat image_format) {
	VkDeviceSize buffer_size = 0;
	VkExtent3D copy_extent = region.imageExtent;
	VkDeviceSize buffer_width = (0 == region.bufferRowLength ? copy_extent.width : region.bufferRowLength);
	VkDeviceSize buffer_height = (0 == region.bufferImageHeight ? copy_extent.height : region.bufferImageHeight);
	VkDeviceSize unit_size = FormatElementSize(image_format,
											   region.imageSubresource.aspectMask);  // size (bytes) of texel or block

	if (FormatIsCompressed(image_format) || FormatIsSinglePlane_422(image_format)) {
		// Switch to texel block units, rounding up for any partially-used blocks
		auto block_dim = FormatTexelBlockExtent(image_format);
		buffer_width = (buffer_width + block_dim.width - 1) / block_dim.width;
		buffer_height = (buffer_height + block_dim.height - 1) / block_dim.height;

		copy_extent.width = (copy_extent.width + block_dim.width - 1) / block_dim.width;
		copy_extent.height = (copy_extent.height + block_dim.height - 1) / block_dim.height;
		copy_extent.depth = (copy_extent.depth + block_dim.depth - 1) / block_dim.depth;
	}

	// Either depth or layerCount may be greater than 1 (not both). This is the number of 'slices' to copy
	uint32_t z_copies = std::max(copy_extent.depth, region.imageSubresource.layerCount);
	if (IsExtentSizeZero(&copy_extent) || (0 == z_copies)) {
		// TODO: Issue warning here? Already warned in ValidateImageBounds()...
	} else {
		// Calculate buffer offset of final copied byte, + 1.
		buffer_size = (z_copies - 1) * buffer_height * buffer_width;                   // offset to slice
		buffer_size += ((copy_extent.height - 1) * buffer_width) + copy_extent.width;  // add row,col
		buffer_size *= unit_size;                                                      // convert to bytes
	}
	return buffer_size;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBufferToImage(
	VkCommandBuffer                             commandBuffer,
	VkBuffer                                    srcBuffer,
	VkImage                                     dstImage,
	VkImageLayout                               dstImageLayout,
	uint32_t                                    regionCount,
	const VkBufferImageCopy*                    pRegions)
{
	auto width = pRegions[0].imageExtent.width;
	auto height = pRegions[0].imageExtent.height;

	CheekyLayer::active_logger log = *logger << logger::begin;
	log << "CmdCopyBufferToImage: src=" << srcBuffer << " @ " << std::hex << pRegions[0].bufferOffset << std::dec
			<< " (" << pRegions[0].bufferRowLength << "x" << pRegions[0].bufferImageHeight
			<< ") dst=" << dstImage <<
			" @ " << width << "x" << height << "#" << pRegions[0].imageExtent.depth;

	VkDevice device = bufferDevices[srcBuffer];
	VkDeviceMemory memory = bufferMemories[srcBuffer];
	VkLayerDispatchTable dispatch = device_dispatch[GetKey(device)];
	VkImageCreateInfo imgInfo = images[dstImage];

	auto format = imgInfo.format;
	log << " format=" << format;

	auto offset = bufferMemoryOffsets[srcBuffer] + pRegions[0].bufferOffset;
	auto size = GetBufferSizeFromCopyImage(pRegions[0], format);
	log << " size=" << size;

	void* data;
	VkResult ret = dispatch.MapMemory(device, memory, offset, size, 0, &data);
	if(ret == VK_SUCCESS)
	{
		char hash[65];
		sha256_string((uint8_t*)data, (size_t)size, hash);
		std::string hash_string(hash);

		if(pRegions[0].imageSubresource.mipLevel == 0)
		{
			CheekyLayer::rule_env.hashes[(VkHandle)dstImage] = hash_string;
			CheekyLayer::local_context ctx = {log};
			CheekyLayer::execute_rules(rules, CheekyLayer::selector_type::Image, (VkHandle)dstImage, ctx);
		}

		log << " hash=" << hash;
		if(global_config.map<bool>("dump", CheekyLayer::config::to_bool))
		{
			std::ofstream out(global_config["dumpDirectory"]+"/images/"+hash_string+".image", std::ios_base::binary);
			if(out.good())
				out.write((char*)data, (size_t)size);

#if defined(USE_IMAGE_TOOLS) && defined(EXPORT_PNG)
			if(image_tools::is_decompression_supported(imgInfo.format))
			{
				std::string dir = global_config["dumpDirectory"]+"/images/png/"+std::to_string(width)+"x"+std::to_string(height)+"/";
				std::string outputPath = dir+hash_string+".png";
				if(!std::filesystem::exists(outputPath))
				{
					try
					{
						image_tools::image image(width, height);
						image_tools::decompress(imgInfo.format, (uint8_t*)data, image, width, height);

						std::filesystem::create_directories(dir);
						stbi_write_png(outputPath.c_str(), width, height, 4, image, width*4);
					}
					catch(std::exception& ex) { log << " Something went wrong: " << ex.what(); }
					catch(...) { log << " Something went really wrong"; }
				}
			}
#endif
		}

		if(global_config.map<bool>("override", CheekyLayer::config::to_bool))
		{
			if(has_override(hash_string))
			{
				std::ifstream in(global_config["overrideDirectory"]+"/images/"+hash_string+".image", std::ios_base::binary);
				if(in.good())
				{
					log << " Found image override!";
					in.read((char*)data, size);
				}
#ifdef USE_IMAGE_TOOLS
				else if(image_tools::is_compression_supported(imgInfo.format))
				{
					std::string path = global_config["overrideDirectory"]+"/images/"+hash_string+".png";
					if(std::filesystem::exists(path))
					{
						try
						{
							int w, h, comp;
							uint8_t* buf = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);

							image_tools::image image(w, h, buf);
							std::vector<uint8_t> out;

							image_tools::compress(format, image, out, width, height);
							std::copy(out.begin(), out.end(), (uint8_t*)data);

							if(pRegions[0].imageSubresource.mipLevel == 0)
							{
								topResolutions[dstImage] = std::make_unique<image_tools::image>(std::move(image));
								log << " This seems to be the best resolution?";
							}
							free(buf);
							log << " Found and converted image override to format " << format;
						}
						catch(std::exception& ex) { log << " Something went wrong: " << ex.what(); }
						catch(...) { log << " Something went really wrong"; }
					}
				}
#endif
			}
#ifdef USE_IMAGE_TOOLS
			else
			{
				auto it = topResolutions.find(dstImage);
				if(it != topResolutions.end())
				{
					try
					{
						std::vector<uint8_t> out;

						image_tools::compress(format, *it->second.get(), out, width, height);
						std::copy(out.begin(), out.end(), (uint8_t*)data);

						log << " Found and converted image override of top resolution to format " << format;
					}
					catch(std::exception& ex) { log << " Something went wrong: " << ex.what(); }
					catch(...) { log << " Something went really wrong"; }
				}
			}
#endif
		}

		dispatch.UnmapMemory(device, memory);
	}
	else
	{
		log << " Cannot map memory " << ret;
	}
	log << logger::end;

	dispatch.CmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}
