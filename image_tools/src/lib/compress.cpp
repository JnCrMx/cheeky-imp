#include "block_compression.hpp"
#include "image.hpp"

#include <bits/stdint-uintn.h>
#include <glm/fwd.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <bc7enc16.h>

namespace image_tools
{
	unsigned short shortcolor(glm::vec4 rgba)
	{
		int r = (int)(rgba.r*31.0f) & 31;
		int g = (int)(rgba.g*63.0f) & 63;
		int b = (int)(rgba.b*31.0f) & 31;

		return (r<<11 | g<<5 | b);
	}

	void writeColorBlock(const std::array<float, 16>& in, uint8_t* out)
	{
		auto [minp, maxp] = std::minmax_element(in.begin(), in.end());
		float min = *minp;
		float max = *maxp;

		if(max == min)
		{
			if(max == 0.0)
				max = 0.1;
			else
				min *= 0.75;
		}

		out[0] = max*255.0f;
		out[1] = min*255.0f;

		std::array<float, 8> palette = {
			max,
			min,
			(6*max + 1*min)/7,
			(5*max + 2*min)/7,
			(4*max + 3*min)/7,
			(3*max + 4*min)/7,
			(2*max + 5*min)/7,
			(1*max + 6*min)/7
		};

		uint8_t* lookup = out + 2;
		for(int row=0; row<2; row++)
		{
			uint32_t b = 0;
			for(int col=0; col<8; col++)
			{
				int x = (col%4);
				int y = (2*row + col/4);

				float val = in[4*y + x];
				int index = std::distance(palette.begin(), std::min_element(palette.begin(), palette.end(), [val](float a, float b){
					return std::abs(a-val) < std::abs(b-val);
				}));

				b |= index << (3*col);
			}
			lookup[3*row + 2] = (b >> (0*8)) & 0xff;
			lookup[3*row + 1] = (b >> (1*8)) & 0xff;
			lookup[3*row + 0] = (b >> (3*8)) & 0xff;
		}
	}

	void compressBC13(const image& in, std::vector<uint8_t>& out, int w, int h, bool bc3)
	{
		out.resize(w * h / (bc3?1:2));
		std::fill(out.begin(), out.end(), 0x00);

		uint8_t* buf = out.data();

		for(int y=0; y<h; y+=4)
		{
			for(int x=0; x<w; x+=4)
			{
				std::array<glm::vec4, 16> colors;
				for(int xx=0; xx<4; xx++)
					for(int yy=0; yy<4; yy++)
						colors[4*yy+xx] = in.scaled(x+xx, y+yy, w, h);

				if(bc3)
				{
					std::array<float, 16> alpha;
					std::transform(colors.begin(), colors.end(), alpha.begin(), [](glm::vec4 color){
						return color.w;
					});
					writeColorBlock(alpha, buf);
					buf += 8;
				}

				auto [minp, maxp] = std::minmax_element(colors.begin(), colors.end(), [](glm::vec4 a, glm::vec4 b){
					return shortcolor(a) < shortcolor(b);
				});
				glm::vec4 max = *maxp;
				glm::vec4 min = *minp;

				if(shortcolor(min) == shortcolor(max))
				{
					if(glm::length(glm::vec3(max)) < 0.1)
						max += glm::vec4(0.25, 0.25, 0.25, 0.0);
					else
						min *= glm::vec4(0.5, 0.5, 0.5, 1.0);
				}

				if(shortcolor(max) <= shortcolor(min))
					std::cout << shortcolor(max) << " " << glm::to_string(max) << " > " << shortcolor(min) << " " << glm::to_string(min) << std::endl;

				((uint16_t*)buf)[0] = shortcolor(max);
				((uint16_t*)buf)[1] = shortcolor(min);
				buf += 2*sizeof(uint16_t);

				glm::vec3 v0 = max;
				glm::vec3 v1 = min;
				glm::vec3 v2 = (2.0f/3.0f) * v0 + (1.0f/3.0f) * v1;
				glm::vec3 v3 = (1.0f/3.0f) * v0 + (2.0f/3.0f) * v1;
				std::array<glm::vec3, 4> palette = {v0, v1, v2, v3};

				for(int yy=0; yy<4; yy++)
				{
					unsigned char b = 0;
					for(int xx=0; xx<4; xx++)
					{
						glm::vec3 trueColor = colors[yy*4+xx];
						auto index = std::distance(palette.begin(), std::min_element(palette.begin(), palette.end(), [trueColor](glm::vec3 a, glm::vec3 b){
							float da = glm::distance(a, trueColor);
							float db = glm::distance(b, trueColor);
							return da < db;
						}));
						b |= index << xx*2;
					}
					buf[yy] = b;
				}
				buf+=4;
			}
		}
	}

	void compressBC1(const image& in, std::vector<uint8_t>& out, int w, int h)
	{
		compressBC13(in, out, w, h, false);
	}

	void compressBC3(const image& in, std::vector<uint8_t>& out, int w, int h)
	{
		compressBC13(in, out, w, h, true);
	}

	void compressBC4(const image &in, std::vector<uint8_t> &out, int w, int h)
	{
		out.resize(w * h * 0.5);
		std::fill(out.begin(), out.end(), 0x00);

		uint8_t* buf = out.data();
		for(int y=0; y<h; y+=4)
		{
			for(int x = 0; x < w; x += 4)
			{
				std::array<glm::vec4, 16> colors;
				for(int xx=0; xx<4; xx++)
					for(int yy=0; yy<4; yy++)
						colors[4*yy+xx] = in.scaled(x+xx, y+yy, w, h);

				std::array<float, 16> red;
				std::transform(colors.begin(), colors.end(), red.begin(), [](glm::vec4 color){
					return color.r;
				});
				writeColorBlock(red, buf);
				buf += 8;
			}
		}
	}

	void compressBC5(const image &in, std::vector<uint8_t> &out, int w, int h)
	{
		out.resize(w * h);
		std::fill(out.begin(), out.end(), 0x00);

		uint8_t* buf = out.data();
		for(int y=0; y<h; y+=4)
		{
			for(int x = 0; x < w; x += 4)
			{
				std::array<glm::vec4, 16> colors;
				for(int xx=0; xx<4; xx++)
					for(int yy=0; yy<4; yy++)
						colors[4*yy+xx] = in.scaled(x+xx, y+yy, w, h);

				std::array<float, 16> red;
				std::transform(colors.begin(), colors.end(), red.begin(), [](glm::vec4 color){
					return color.r;
				});
				writeColorBlock(red, buf);
				buf += 8;

				std::array<float, 16> green;
				std::transform(colors.begin(), colors.end(), green.begin(), [](glm::vec4 color){
					return color.g;
				});
				writeColorBlock(green, buf);
				buf += 8;
			}
		}
	}

	static bool bc7_init = false;
	void compressBC7(const image &in, std::vector<uint8_t> &out, int w, int h)
	{
		out.resize(w * h);
		std::fill(out.begin(), out.end(), 0x00);

		if(!bc7_init)
		{
			bc7enc16_compress_block_init();
			bc7_init = true;
		}

		bc7enc16_compress_block_params params;
		bc7enc16_compress_block_params_init(&params);
		bc7enc16_compress_block_params_init_perceptual_weights(&params);

		for(int y=0; y<h; y+=4)
		{
			for(int x=0; x<w; x+=4)
			{
				uint8_t rgba[4*4*4];
				for(int yy=0; yy<4; yy++)
				{
					for(int xx=0; xx<4; xx++)
					{
						glm::vec4 v = in.scaled(x+xx, y+yy, w, h);
						rgba[yy*4*4 + xx*4 + 0] = v.r * 255.0f;
						rgba[yy*4*4 + xx*4 + 1] = v.g * 255.0f;
						rgba[yy*4*4 + xx*4 + 2] = v.b * 255.0f;
						rgba[yy*4*4 + xx*4 + 3] = v.a * 255.0f;
					}
				}

				int offset = (y*w + x*4);
				uint8_t* outPointer = out.data() + offset;
				bc7enc16_compress_block(outPointer, rgba, &params);
			}
		}
	}
}
