#include "layer.hpp"
#include "utils.hpp"
#include "objects.hpp"

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

namespace CheekyLayer {

VkResult device::CreateImage(const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
{
	VkResult ret = dispatch.CreateImage(handle, pCreateInfo, pAllocator, pImage);
	if(ret != VK_SUCCESS)
	{
		logger->warn("CreateImage: {}x{} @ {} samples={} FAILED!", pCreateInfo->extent.width, pCreateInfo->extent.height,
				pCreateInfo->mipLevels, fmt::underlying(pCreateInfo->samples));
		return ret;
	}
	auto& image = images[*pImage];
	image.image = *pImage;
	image.createInfo = *pCreateInfo;

	VkMemoryRequirements memRequirements;
	dispatch.GetImageMemoryRequirements(handle, *pImage, &memRequirements);

	logger->debug("CreateImage: {}x{} @ {} samples={} memory={}, image={}", pCreateInfo->extent.width, pCreateInfo->extent.height,
			pCreateInfo->mipLevels, fmt::underlying(pCreateInfo->samples), memRequirements.size, fmt::ptr(*pImage));

	if(has_debug)
	{
		std::string name = fmt::format("Image {:#x}", reinterpret_cast<uint64_t>(*pImage));

		VkDebugUtilsObjectNameInfoEXT info{};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		info.objectType = VK_OBJECT_TYPE_IMAGE;
		info.objectHandle = (uint64_t) *pImage;
		info.pObjectName = name.c_str();
		dispatch.SetDebugUtilsObjectNameEXT(handle, &info);
	}
	return ret;
}

VkResult device::BindImageMemory(VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
	images[image].memory = memory;
	images[image].memoryOffset = memoryOffset;

	VkResult ret = dispatch.BindImageMemory(handle, image, memory, memoryOffset);
	logger->debug("BindImageMemory: image={} memory={} offset={:#x}", fmt::ptr(image), fmt::ptr(memory), memoryOffset);
	return ret;
}

VkResult device::CreateImageView(const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView)
{
	VkResult ret = dispatch.CreateImageView(handle, pCreateInfo, pAllocator, pView);
	if(ret == VK_SUCCESS) {
		images[pCreateInfo->image].view = *pView;
		imageViewToImage[*pView] = pCreateInfo->image;
	}
	return ret;
}

void device::handle_buffer_image_copy(std::string_view command, unsigned int regionIndex, VkCommandBuffer commandBuffer, VkBuffer src, VkImage dst, VkBufferImageCopy region) {
	auto width = region.imageExtent.width;
	auto height = region.imageExtent.height;

	if(!buffers.contains(src)) {
		logger->error("handle_buffer_image_copy: src buffer {} not found!", fmt::ptr(src));
	}
	if(!images.contains(dst)) {
		logger->error("handle_buffer_image_copy: dst image {} not found!", fmt::ptr(dst));
	}
	auto& image = images[dst];

	auto format = image.createInfo.format;
	auto size = GetBufferSizeFromCopyImage(region, format);

	try {
		memory_access(src, [&](void* data, VkDeviceSize mapped_size){
			if(mapped_size < size) {
				throw std::runtime_error("mapped memory size is smaller than expected");
			}

			std::string hash_string = sha256_string((uint8_t*)data, (size_t)size);
			logger->info("{}[{}]: src={} @ {:#x} ({}x{} # {}) dst={} @ {}x{}#{}, format={}, size={}, hash={}", command, regionIndex, fmt::ptr(src), region.bufferOffset,
				region.bufferRowLength, region.bufferImageHeight, region.imageSubresource.mipLevel, fmt::ptr(dst),
				width, height, region.imageExtent.depth, fmt::underlying(format), size, hash_string);
			bool is_high_res = region.imageSubresource.mipLevel == 0;

			if(is_high_res) {
				put_hash((rules::VkHandle)dst, hash_string);
				{
					rules::calling_context ctx{
						.local_variables = {
							{"image:hash", hash_string},
							{"image:width", static_cast<double>(width)},
							{"image:height", static_cast<double>(height)},
							{"image:format", vk::to_string(vk::Format(format))},
							{"image:format_raw", static_cast<double>(fmt::underlying(format))},
							{"image:size", static_cast<double>(size)},
						}
					};
					execute_rules(rules::selector_type::Image, (rules::VkHandle)dst, ctx);
				}

				if(has_debug) {
					std::string name = "Image "+hash_string;

					VkDebugUtilsObjectNameInfoEXT info{};
					info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					info.objectType = VK_OBJECT_TYPE_IMAGE;
					info.objectHandle = (uint64_t) dst;
					info.pObjectName = name.c_str();
					if(dispatch.SetDebugUtilsObjectNameEXT(handle, &info) != VK_SUCCESS) {
						logger->warn("Failed to set debug name on image {}", fmt::ptr(dst));
					}
				}
			}

			if(inst->config.dump) {
				{
					auto outputPath = inst->config.dump_directory / "images" / (hash_string+".image");
					std::ofstream out(outputPath, std::ios_base::binary);
					if(out.good())
						out.write((char*)data, (size_t)size);
				}
#ifdef USE_IMAGE_TOOLS
				if(is_high_res && inst->config.dump_png && size >= 256) {
					if(image_tools::is_decompression_supported(image.createInfo.format)) {
						auto outputPath = inst->config.dump_directory / "images" / "png" /
							fmt::format("{}x{}", width, height) / (hash_string+".png");
						if(!std::filesystem::exists(outputPath)) {
							try {
								image_tools::image img(width, height);
								image_tools::decompress(image.createInfo.format, (uint8_t*)data, img, width, height);
								if(inst->config.dump_png_flipped) {
									image_tools::image flipped(width, height);
									for(int y = 0; y < height; y++) {
										for(int x = 0; x < width; x++) {
											flipped.at(x, height-y-1) = img.at(x, y);
										}
									}
									img = std::move(flipped);
								}
								std::filesystem::create_directories(outputPath.parent_path());
								stbi_write_png(outputPath.c_str(), width, height, 4, img, width*4);
							} catch(std::exception& ex) {
								logger->error("Something went wrong: {}", ex.what());
							} catch(...) {
								logger->error("Something went really wrong");
							}
						}
					} else {
						logger->warn("Cannot export PNG for format {}", fmt::underlying(image.createInfo.format));
					}
				}
#endif
			}

			if(inst->config.override) {
				if(has_override(hash_string)) {
					auto rawPath = inst->config.override_directory / "images" / (hash_string+".image");
					auto pngPath = inst->config.override_directory / "images" / (hash_string+".png");
					if(std::filesystem::exists(rawPath)) {
						logger->info("Found image override at {}!", rawPath.string());
						std::ifstream in(rawPath, std::ios_base::binary);
						in.read((char*)data, size);
					}
#ifdef USE_IMAGE_TOOLS
					else if(std::filesystem::exists(pngPath)) {
						if(image_tools::is_compression_supported(image.createInfo.format)) {
							try {
								int w, h, comp;
								uint8_t* buf = stbi_load(pngPath.c_str(), &w, &h, &comp, STBI_rgb_alpha);
								image_tools::image img(w, h, buf);
								if(inst->config.override_png_flipped) {
									image_tools::image flipped(w, h);
									for(int y = 0; y < h; y++) {
										for(int x = 0; x < w; x++) {
											flipped.at(x, h-y-1) = img.at(x, y);
										}
									}
									img = std::move(flipped);
								}
								std::vector<uint8_t> out;
								image_tools::compress(image.createInfo.format, img, out, width, height);
								std::copy(out.begin(), out.end(), (uint8_t*)data);
								if(is_high_res) {
									image.topResolution = std::make_unique<image_tools::image>(std::move(img));
								}
								free(buf);
								logger->info("Found and converted image override to format {}", fmt::underlying(image.createInfo.format));
							} catch(std::exception& ex) {
								logger->error("Something went wrong: {}", ex.what());
							} catch(...) {
								logger->error("Something went really wrong");
							}
						} else {
							logger->warn("Cannot import PNG for format {}", fmt::underlying(image.createInfo.format));
						}
					}
#endif
				}
#ifdef USE_IMAGE_TOOLS
				else if(image.topResolution) {
					try {
						std::vector<uint8_t> out;
						image_tools::compress(image.createInfo.format, *image.topResolution.get(), out, width, height);
						std::copy(out.begin(), out.end(), (uint8_t*)data);
						logger->info("Found and converted image override of top resolution to format {}", fmt::underlying(image.createInfo.format));
					} catch(std::exception& ex) {
						logger->error("Something went wrong: {}", ex.what());
					} catch(...) {
						logger->error("Something went really wrong");
					}
				}
#endif
			}
		}, region.bufferOffset);
	} catch (const std::exception& e) {
		logger->info("{}[{}]: src={} @ {:#x} ({}x{} # {}) dst={} @ {}x{}#{}, format={}, size={}", command, regionIndex, fmt::ptr(src), region.bufferOffset,
			region.bufferRowLength, region.bufferImageHeight, region.imageSubresource.mipLevel, fmt::ptr(dst),
			width, height, region.imageExtent.depth, fmt::underlying(format), size);
		logger->warn("Cannot access memory: {}", e.what());
	}
}

void device::CmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
	for(uint32_t i = 0; i < regionCount; i++) {
		handle_buffer_image_copy("CmdCopyBufferToImage", i, commandBuffer, srcBuffer, dstImage, pRegions[i]);
	}
	dispatch.CmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

void device::CmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo)
{
	for(uint32_t i = 0; i < pCopyBufferToImageInfo->regionCount; i++) {
		if(pCopyBufferToImageInfo->pRegions[i].pNext) {
			logger->warn("CmdCopyBufferToImage2: pRegions[{}].pNext is not null!", i);
		}
		VkBufferImageCopy region{
			.bufferOffset = pCopyBufferToImageInfo->pRegions[i].bufferOffset,
			.bufferRowLength = pCopyBufferToImageInfo->pRegions[i].bufferRowLength,
			.bufferImageHeight = pCopyBufferToImageInfo->pRegions[i].bufferImageHeight,
			.imageSubresource = pCopyBufferToImageInfo->pRegions[i].imageSubresource,
			.imageOffset = pCopyBufferToImageInfo->pRegions[i].imageOffset,
			.imageExtent = pCopyBufferToImageInfo->pRegions[i].imageExtent,
		};
		handle_buffer_image_copy("CmdCopyBufferToImage2", i, commandBuffer, pCopyBufferToImageInfo->srcBuffer, pCopyBufferToImageInfo->dstImage, region);
	}
	dispatch.CmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo);
}

}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateImage(
	VkDevice                                    device,
	const VkImageCreateInfo*                    pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkImage*                                    pImage)
{
	return CheekyLayer::get_device(device).CreateImage(pCreateInfo, pAllocator, pImage);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_BindImageMemory(
	VkDevice                                    device,
	VkImage                                     image,
	VkDeviceMemory                              memory,
	VkDeviceSize                                memoryOffset)
{
	return CheekyLayer::get_device(device).BindImageMemory(image, memory, memoryOffset);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateImageView(
	VkDevice                                    device,
	const VkImageViewCreateInfo*                pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkImageView*                                pView)
{
	return CheekyLayer::get_device(device).CreateImageView(pCreateInfo, pAllocator, pView);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBufferToImage(
	VkCommandBuffer                             commandBuffer,
	VkBuffer                                    srcBuffer,
	VkImage                                     dstImage,
	VkImageLayout                               dstImageLayout,
	uint32_t                                    regionCount,
	const VkBufferImageCopy*                    pRegions)
{
	return CheekyLayer::get_device(commandBuffer).CmdCopyBufferToImage(commandBuffer,
		srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdCopyBufferToImage2(
	VkCommandBuffer                             commandBuffer,
	const VkCopyBufferToImageInfo2*             pCopyBufferToImageInfo)
{
	return CheekyLayer::get_device(commandBuffer).CmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo);
}
