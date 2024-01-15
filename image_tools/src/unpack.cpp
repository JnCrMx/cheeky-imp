#include <bits/stdint-uintn.h>
#include <fstream>
#include <string>
#include <iostream>

#include "block_compression.hpp"
#include "image.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

using namespace image_tools;

int main(int argc, char* argv[])
{
	if(argc != 6)
	{
		std::cerr << "Usage: " << argv[0] << " [format] [input-file] [output-file] [width] [height]" << std::endl;
		return 2;
	}

	std::string format		= argv[1];
	std::string inputPath	= argv[2];
	std::string outputPath	= argv[3];
	int width				= std::atoi(argv[4]);
	int height				= std::atoi(argv[5]);

	std::vector<uint8_t> data;

	{
		std::ifstream in(inputPath, std::ios::binary | std::ios::ate);
		if(!in)
		{
			std::cerr << "Cannot open file: " << inputPath << std::endl;
			return 2;
		}
		
		size_t size = in.tellg();
		data.resize(size);
		in.seekg(0);

		in.read((char*)data.data(), size);
	}

	image image(width, height);

	if(format=="BC1")
	{
		decompressBC1(data.data(), image, width, height);
	}
	else if(format=="BC2")
	{
		decompressBC2(data.data(), image, width, height);
	}
	else if(format=="BC3")
	{
		decompressBC3(data.data(), image, width, height);
	}
	else if(format=="BC4")
	{
		decompressBC4(data.data(), image, width, height);
	}
	else if(format=="BC5")
	{
		decompressBC5(data.data(), image, width, height);
	}
	else if(format=="BC7")
	{
		decompressBC7(data.data(), image, width, height, true);
	}
	else if(format=="BC7-noalpha")
	{
		decompressBC7(data.data(), image, width, height, false);
	}

	stbi_write_png(outputPath.c_str(), width, height, 4, image, width*4);

	return 0;
}
