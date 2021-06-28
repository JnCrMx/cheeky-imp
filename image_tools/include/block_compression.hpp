#pragma once
#include <bits/stdint-uintn.h>
#include "image.hpp"

namespace image_tools
{
	void compressBC1(const image& in, std::vector<uint8_t>& out, int w, int h);
	void compressBC3(const image& in, std::vector<uint8_t>& out, int w, int h);
	void compressBC4(const image& in, std::vector<uint8_t>& out, int w, int h);
	void compressBC5(const image& in, std::vector<uint8_t>& out, int w, int h);

	void decompressBC1(const uint8_t* in, image& out, int w, int h);
	void decompressBC3(const uint8_t* in, image& out, int w, int h);
	void decompressBC4(const uint8_t* in, image& out, int w, int h);
	void decompressBC5(const uint8_t* in, image& out, int w, int h);
}