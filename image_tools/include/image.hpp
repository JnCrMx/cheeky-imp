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
			image(int w, int h, std::vector<unsigned char>&& d);
			image(int w, int h, uint8_t* pointer);

			color& at(int x, int y);
			glm::vec4 scaled(int x, int y, int w, int h) const;

			operator const uint8_t*();

			std::vector<color>::iterator begin();
			std::vector<color>::iterator end();

			std::vector<color>::const_iterator cbegin() const;
			std::vector<color>::const_iterator cend() const;
		private:
			std::vector<color> data;
			int width;
			int height;
	};

	image::color color(glm::vec4);
	glm::vec4 color_to_vec4(image::color);
}
