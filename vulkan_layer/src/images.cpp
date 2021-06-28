#include "layer.hpp"
#include "dispatch.hpp"
#include "buffers.hpp"
#include "utils.hpp"

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

using CheekyLayer::logger;

std::map<VkImage, VkImageCreateInfo> images;

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

		log << " hash=" << hash;
		if(global_config.map<bool>("dump", CheekyLayer::config::to_bool))
		{
			std::ofstream out(global_config["dumpDirectory"]+"/images/"+hash_string+".image", std::ios_base::binary);
			if(out.good())
				out.write((char*)data, (size_t)size);
		}

		if(global_config.map<bool>("override", CheekyLayer::config::to_bool) && has_override(hash_string))
		{
			std::ifstream in(global_config["overrideDirectory"]+"/images/"+hash_string+".image", std::ios_base::binary);
			if(in.good())
			{
				log << " Found image override!";
				in.read((char*)data, size);
			}
#ifdef USE_IMAGE_TOOLS
			else if(imgInfo.format == VK_FORMAT_BC1_RGBA_SRGB_BLOCK ||
					imgInfo.format == VK_FORMAT_BC3_SRGB_BLOCK)
			{
				std::string path = global_config["overrideDirectory"]+"/images/"+hash_string+".png";
				if(std::filesystem::exists(path))
				{
					int w, h, comp;
					uint8_t* buf = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);

					std::vector<unsigned char> imagedata(buf, buf+w*h*4);
					image_tools::image image(w, h, imagedata);
					std::vector<uint8_t> out;

					bool ok = true;
					switch(imgInfo.format)
					{
						case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
							image_tools::compressBC1(image, out, imgInfo.extent.width, imgInfo.extent.height);
							break;
						case VK_FORMAT_BC3_SRGB_BLOCK:
							image_tools::compressBC3(image, out, imgInfo.extent.width, imgInfo.extent.height);
							break;
						default:
							log << " Cannot convert image override to format " << imgInfo.format;
							ok = false;
					}

					if(ok)
					{
						log << " Found and converted image override to format " << imgInfo.format;
						std::copy(out.begin(), out.end(), (uint8_t*)data);
					}
					free(buf);
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
