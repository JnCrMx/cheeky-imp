#include <bits/stdint-uintn.h>
#include <fstream>
#include <ios>
#include <string>
#include <assert.h>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "block_compression.hpp"
#include "image.hpp"

#include <iostream>

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

	int w, h, comp;
	uint8_t* buf = stbi_load(inputPath.c_str(), &w, &h, &comp, STBI_rgb_alpha);
	
	image image(w, h, buf);
	std::vector<uint8_t> out;

	if(format=="BC1")
	{
		compressBC1(image, out, width, height);
	}
	else if(format=="BC3")
	{
		compressBC3(image, out, width, height);
	}
	else if(format=="BC4")
	{
		compressBC4(image, out, width, height);
	}
	else if(format=="BC5")
	{
		compressBC5(image, out, width, height);
	}
	else if(format=="BC7" || format=="BC7-noalpha")
	{
		compressBC7(image, out, width, height);
	}

	{
		std::ofstream outStream(outputPath, std::ios::binary);
		outStream.write((char*)out.data(), out.size());
		outStream.flush();
	}

	return 0;
}
