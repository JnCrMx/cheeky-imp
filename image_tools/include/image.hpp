#pragma once

#include <bits/stdint-uintn.h>
#include <glm/glm.hpp>
#include <vector>

namespace image_tools
{
	class image
	{
		public:
			typedef unsigned int color;
			image(int width, int height);
			image(int w, int h, std::vector<unsigned char> d);

			color& at(int x, int y);
			glm::vec4 scaled(int x, int y, int w, int h) const;

			operator const uint8_t*();

		private:
			std::vector<color> data;
			int width;
			int height;
	};

	image::color color(glm::vec4);
	glm::vec4 color_to_vec4(image::color);
}
