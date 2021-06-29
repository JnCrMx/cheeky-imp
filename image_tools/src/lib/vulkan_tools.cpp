#ifdef WITH_VULKAN

#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "block_compression.hpp"

namespace image_tools
{
	bool is_compression_supported(VkFormat format)
	{
		switch(format)
		{
			case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
			case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
				return true;
			case VK_FORMAT_BC3_SRGB_BLOCK:
				return true;
			case VK_FORMAT_BC7_SRGB_BLOCK:
				return true;
			default:
				return false;
		}
	}
	bool is_compression_supported(vk::Format format)
	{
		return is_compression_supported((VkFormat)format);
	}

	bool is_decompression_supported(VkFormat format)
	{
		switch(format)
		{
			case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
			case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
				return true;
			case VK_FORMAT_BC3_SRGB_BLOCK:
				return true;
			case VK_FORMAT_BC7_SRGB_BLOCK:
				return true;
			default:
				return false;
		}
	}
	bool is_decompression_supported(vk::Format format)
	{
		return is_decompression_supported((VkFormat)format);
	}

	void compress(VkFormat format, const image& in, std::vector<uint8_t>& out, int w, int h)
	{
		switch(format)
		{
			case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
			case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
				compressBC1(in, out, w, h);
				break;
			case VK_FORMAT_BC3_SRGB_BLOCK:
				compressBC3(in, out, w, h);
				break;
			case VK_FORMAT_BC7_SRGB_BLOCK:
				compressBC7(in, out, w, h);
				break;
			default:
				throw std::runtime_error("format not supported: "+vk::to_string(vk::Format(format)));
		}
	}
	void compress(vk::Format format, const image& in, std::vector<uint8_t>& out, int w, int h)
	{
		compress((VkFormat)format, in, out, w, h);
	}

	void decompress(VkFormat format, const uint8_t* in, image& out, int w, int h)
	{
		switch(format)
		{
			case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
			case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
				decompressBC1(in, out, w, h);
				break;
			case VK_FORMAT_BC3_SRGB_BLOCK:
				decompressBC3(in, out, w, h);
				break;
			case VK_FORMAT_BC7_SRGB_BLOCK:
				decompressBC7(in, out, w, h, false);
				break;
			default:
				throw std::runtime_error("format not supported: "+vk::to_string(vk::Format(format)));
		}
	}
	void decompress(vk::Format format, const uint8_t *in, image &out, int w, int h)
	{
		decompress((VkFormat)format, in, out, w, h);
	}
}
#endif