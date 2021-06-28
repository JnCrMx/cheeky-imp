#include "block_compression.hpp"
#include "image.hpp"

#include <algorithm>
#include <bits/stdint-uintn.h>
#include <bitset>
#include <glm/fwd.hpp>
#include <iostream>

namespace image_tools
{
	void readColorBlock(const uint8_t* in, float out[4][4])
	{
		float colors[8];
		float c0 = in[0]/255.0f;
		float c1 = in[1]/255.0f;

		colors[0] = c0;
		colors[1] = c1;

		if(c0 > c1)
		{
			colors[2] = (6.0f * c0 + 1.0f * c1) / 7.0f;
			colors[3] = (5.0f * c0 + 2.0f * c1) / 7.0f;
			colors[4] = (4.0f * c0 + 3.0f * c1) / 7.0f;
			colors[5] = (3.0f * c0 + 4.0f * c1) / 7.0f;
			colors[6] = (2.0f * c0 + 5.0f * c1) / 7.0f;
			colors[7] = (1.0f * c0 + 6.0f * c1) / 7.0f;
		}
		else
		{
			colors[2] = (4.0f * c0 + 1.0f * c1) / 5.0f;
			colors[3] = (3.0f * c0 + 2.0f * c1) / 5.0f;
			colors[4] = (2.0f * c0 + 3.0f * c1) / 5.0f;
			colors[5] = (1.0f * c0 + 4.0f * c1) / 5.0f;
			colors[6] = 0.0f;
			colors[7] = 1.0f;
		}

		uint32_t lookup[2] = {
			static_cast<uint32_t>(in[4] << 16 | in[3] << 8 | in[2]),
			static_cast<uint32_t>(in[7] << 16 | in[6] << 8 | in[5]),
		};
		for(int i=0; i<16; i++)
		{
			unsigned int index = (lookup[i/8] >> (3*(i%8))) & 0b111;

			int x = i % 4;
			int y = i / 4;
			out[x][y] = colors[index];
		}
	}

	glm::vec3 color(unsigned short s)
	{
		float r = ((s>>11)&0b011111)/(31.0f);
		float g = ((s>>5)&0b111111)/(63.0f);
		float b = ((s)&0b011111)/(31.0f);

		return {r, g, b};
	}

	void decompressBC13(const uint8_t* in, image& out, int w, int h, bool bc3)
	{
		const uint8_t* buf = in;
		for(int y=0; y<h; y+=4)
		{
			for(int x=0; x<w; x+=4)
			{
				float alpha[4][4];
				if(bc3)
				{
					readColorBlock(buf, alpha);
					buf += 8;
				}
				else
				{
					std::fill(&alpha[0][0], &alpha[4][0], 1.0f);
				}

				unsigned short c0 = ((unsigned short*)(buf))[0];
				unsigned short c1 = ((unsigned short*)(buf))[1];
				buf += 4;

				glm::vec3 v0 = color(c0);
				glm::vec3 v1 = color(c1);
				glm::vec3 v2, v3;
				if(c0 > c1)
				{
					v2 = (2.0f/3.0f) * v0 + (1.0f/3.0f) * v1;
					v3 = (1.0f/3.0f) * v0 + (2.0f/3.0f) * v1;
				}
				else
				{
					v2 = (1.0f/2.0f) * v0 + (1.0f/2.0f) * v1;
					v3 = glm::vec3(0.0f, 0.0f, 0.0f);
				}
				glm::vec3 colors[] = {v0, v1, v2, v3};

				const uint8_t* lookup = buf; buf+=4;
				for(int yy=0; yy<4; yy++)
				{
					uint8_t row = lookup[yy];
					for(int xx=0; xx<4; xx++)
					{
						int index = (row >> (2*xx)) & 0b11;
						out.at(x+xx, y+yy) = color(glm::vec4(colors[index], alpha[xx][yy]));
					}
				}
			}
		}
	}

	void decompressBC1(const uint8_t* in, image& out, int w, int h)
	{
		decompressBC13(in, out, w, h, false);
	}

	void decompressBC3(const uint8_t* in, image& out, int w, int h)
	{
		decompressBC13(in, out, w, h, true);
	}
}