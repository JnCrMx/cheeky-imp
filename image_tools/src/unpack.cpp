#include <bits/stdint-uintn.h>
#include <fstream>
#include <string>
#include <assert.h>

#include "block_compression.hpp"
#include "image.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

using namespace image_tools;

int main(int argc, char* argv[])
{
	assert(argc == 6);
	std::string format		= argv[1];
	std::string inputPath	= argv[2];
	std::string outputPath	= argv[3];
	int width				= std::atoi(argv[4]);
	int height				= std::atoi(argv[5]);

	std::vector<uint8_t> data;

	{
		std::ifstream in(inputPath, std::ios::binary | std::ios::ate);
		
		size_t size = in.tellg();
		data.resize(size);
		in.seekg(0);

		in.read((char*)data.data(), size);
	}

	image image(width, height);

	if(format=="BC3")
	{
		decompressBC3(data.data(), image, width, height);
	}

	stbi_write_png(outputPath.c_str(), width, height, 4, image, width*4);

	return 0;
}
