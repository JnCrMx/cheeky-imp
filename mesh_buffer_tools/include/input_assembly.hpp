#pragma once

#include <fstream>
#include <glm/fwd.hpp>
#include <string>

#include <glm/glm.hpp>

namespace mbt
{
	class input_attribute
	{
		public:
			input_attribute() {}
			input_attribute(int index, std::string name, int location, int binding, std::string format, int offset);

			typedef union {
				glm::vec1 vec1;
				glm::vec2 vec2;
				glm::vec3 vec3;
				glm::vec4 vec4;

				glm::u8vec4 u8vec4;
			} attribute_value;

			attribute_value read(void* pointer);

			int index;
			std::string name;
			int location;
			int binding;
			std::string format;
			int offset;

			friend std::istream& operator>>(std::istream&, input_attribute&);
	};
	std::istream& operator>>(std::istream&, input_attribute&);
}
