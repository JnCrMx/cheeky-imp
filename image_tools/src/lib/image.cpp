#include "image.hpp"
#include <bits/stdint-uintn.h>
#include <cmath>

namespace image_tools
{
	image::image(int w, int h)
	{
		width = w;
		height = h;

		data.resize(w*h);
	}

	image::image(int w, int h, std::vector<unsigned char>&& d)
	{
		width = w;
		height = h;

		data.assign((image::color*)d.data(), ((image::color*)d.data())+w*h);
	}

	image::image(int w, int h, uint8_t* pointer)
	{
		width = w;
		height = h;

		data.assign((image::color*)pointer, ((image::color*)pointer)+w*h);
	}

	image::color& image::at(int x, int y)
	{
		return data.at(y*width + x);
	}

	constexpr glm::ivec4 color_to_ivec4(image::color c)
	{
		int r = (c>>(0*8)) & 0xff;
		int g = (c>>(1*8)) & 0xff;
		int b = (c>>(2*8)) & 0xff;
		int a = (c>>(3*8)) & 0xff;

		return glm::ivec4(r, g, b, a);
	}

	glm::vec4 image::scaled(int x, int y, int w, int h) const
	{
		if(w==width && h==height)
			return color_to_vec4(data[y*width + x]);

		float factorX = width / ((float)w);
		float factorY = height / ((float)h);

		assert(factorX > 1.0);
		assert(factorY > 1.0);

		int xx = x * factorX;
		int yy = y * factorY;

		int scanX = factorX;
		int scanY = factorY;

		glm::ivec4 sum(0.0);
		for(int xxx = xx; xxx < xx+scanX; xxx++)
		{
			for(int yyy = yy; yyy < yy+scanY; yyy++)
			{
				sum += color_to_ivec4(data[yyy*width + xxx]);
			}
		}
		return glm::vec4(sum) / (float)(scanX * scanY * 255.0f);
	}

	image::color color(glm::vec4 rgba)
	{
		int r = rgba.r * 255.0f;
		int g = rgba.g * 255.0f;
		int b = rgba.b * 255.0f;
		int a = rgba.a * 255.0f;

		return (a<<(3*8)) | (b<<(2*8)) | (g<<(1*8)) | (r<<(0*8));
	}

	glm::vec4 color_to_vec4(image::color c)
	{
		int r = (c>>(0*8)) & 0xff;
		int g = (c>>(1*8)) & 0xff;
		int b = (c>>(2*8)) & 0xff;
		int a = (c>>(3*8)) & 0xff;

		return glm::vec4(r/255.0f, g/255.0f, b/255.0f, a/255.0f);
	}

	image::operator const uint8_t*()
	{
		return (const uint8_t*) data.data();
	}

	std::vector<image::color>::iterator image::begin() { return data.begin(); }
	std::vector<image::color>::iterator image::end() { return data.end(); }

	std::vector<image::color>::const_iterator image::cbegin() const { return data.cbegin(); }
	std::vector<image::color>::const_iterator image::cend() const { return data.cend(); }
}
