#pragma once
#include <bits/stdint-uintn.h>
#include "image.hpp"

#ifdef WITH_VULKAN
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#endif

namespace image_tools
{
	void compressBC1(const image& in, std::vector<uint8_t>& out, int w, int h);
	void compressBC3(const image& in, std::vector<uint8_t>& out, int w, int h);
	void compressBC4(const image& in, std::vector<uint8_t>& out, int w, int h);
	void compressBC5(const image& in, std::vector<uint8_t>& out, int w, int h);
	void compressBC7(const image& in, std::vector<uint8_t>& out, int w, int h);

	void decompressBC1(const uint8_t* in, image& out, int w, int h);
	void decompressBC3(const uint8_t* in, image& out, int w, int h);
	void decompressBC4(const uint8_t* in, image& out, int w, int h);
	void decompressBC5(const uint8_t* in, image& out, int w, int h);
	void decompressBC6(const uint8_t* in, image& out, int w, int h);
	void decompressBC7(const uint8_t* in, image& out, int w, int h, bool alpha);

#ifdef WITH_VULKAN
	bool is_compression_supported(VkFormat format);
	bool is_compression_supported(vk::Format format);

	void compress(VkFormat format, const image& in, std::vector<uint8_t>& out, int w, int h);
	void compress(vk::Format format, const image& in, std::vector<uint8_t>& out, int w, int h);

	bool is_decompression_supported(VkFormat format);
	bool is_decompression_supported(vk::Format format);

	void decompress(VkFormat format, const uint8_t* in, image& out, int w, int h);
	void decompress(vk::Format format, const uint8_t* in, image& out, int w, int h);
#endif
}
