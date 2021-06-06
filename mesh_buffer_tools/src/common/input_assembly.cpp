#include "input_assembly.hpp"
#include <bits/stdint-uintn.h>
#include <glm/gtc/packing.hpp>
#include <glm/packing.hpp>
#include <stdexcept>

namespace mbt
{
	input_attribute::input_attribute(int index_, std::string name_, int location_, int binding_, std::string format_, int offset_) :
				index(index_), name(name_), location(location_), binding(binding_), format(format_), offset(offset_)
	{

	}

	std::istream& operator>>(std::istream& is, input_attribute& ia)
	{
		is >> ia.index;
		is >> ia.name;
		is >> ia.location;
		is >> ia.binding;
		is >> ia.format;
		is >> ia.offset;
		return is;
	}

	input_attribute::attribute_value input_attribute::read(void* pointer)
	{
		void* p = ((uint8_t*)pointer) + offset;

		attribute_value val;
		if(format=="R32G32B32_FLOAT")
		{
			val.vec3 = *(glm::vec3*)p;
			return val;
		}
		if(format=="R8G8B8A8_UINT")
		{
			val.u8vec4 = glm::unpackUint4x8(*(glm::uint32*)p);
			return val;
		}
		if(format=="R8G8B8A8_UNORM")
		{
			val.vec4 = glm::unpackUnorm4x8(*(uint*)p);
			return val;
		}
		if(format=="R8G8B8A8_SNORM")
		{
			val.vec4 = glm::unpackSnorm4x8(*(uint*)p);
			return val;
		}
		if(format=="R16G16_FLOAT")
		{
			val.vec2 = glm::unpackHalf2x16(*(uint*)p);
			return val;
		}

		throw std::runtime_error("unknown format");
	}

	void input_attribute::write(attribute_value val, void *pointer)
	{
		void* p = ((uint8_t*)pointer) + offset;

		if(format=="R32G32B32_FLOAT")
		{
			*(glm::vec3*)p = val.vec3;
			return;
		}
		if(format=="R8G8B8A8_UINT")
		{
			*(glm::uint32*)p = glm::packUint4x8(val.u8vec4);
			return;
		}
		if(format=="R8G8B8A8_UNORM")
		{
			*(uint*)p = glm::packUnorm4x8(val.vec4);
			return;
		}
		if(format=="R8G8B8A8_SNORM")
		{
			*(uint*)p = glm::packSnorm4x8(val.vec4);
			return;
		}
		if(format=="R16G16_FLOAT")
		{
			*(uint*)p = glm::packHalf2x16(val.vec2);
			return;
		}

		throw std::runtime_error("unknown format");
	}
}
