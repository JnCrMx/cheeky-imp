#include "reflection/reflectionparser.hpp"
#include "reflection/vkreflection.hpp"

#include <any>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vulkan/vulkan_core.h>

namespace CheekyLayer { namespace reflection {
	bool is_primitive(std::string type)
	{
		if(type=="uint32_t")
			return true;
		if(type=="int32_t")
			return true;
		if(type=="VkBool32")
			return true;
		if(type=="float")
			return true;
		if(enum_reflection_map.contains(type)) // all enums can be returned as an uint32_t
			return true;
		if(type.ends_with("Flags"))
			return true;
		return false;
	}

	std::any create_type(const void* p, std::string type)
	{
		if(type=="uint32_t")
			return *((uint32_t*)p);
		if(type=="int32_t")
			return *((int32_t*)p);
		if(type=="VkBool32")
			return *((VkBool32*)p);
		if(type=="float")
			return *((float*)p);
		if(enum_reflection_map.contains(type)) // all enums can be returned as an uint32_t
			return *((uint32_t*)p);
		if(type.ends_with("Flags"))
			return *((uint32_t*)p);
		throw std::runtime_error("cannot create type \""+type+"\"");
	}

	void set_type(void* p, std::string type, std::any value)
	{
		if(type=="uint32_t" || type.ends_with("Flags"))
			*((uint32_t*)p) = std::any_cast<uint32_t>(value);
		else if(type=="int32_t")
			*((int32_t*)p) = std::any_cast<int32_t>(value);
		else if(type=="VkBool32")
			*((VkBool32*)p) = std::any_cast<VkBool32>(value);
		else if(type=="float")
			*((float*)p) = std::any_cast<float>(value);
		else if(enum_reflection_map.contains(type)) // all enums can be set as an uint32_t
			*((uint32_t*)p) = std::any_cast<uint32_t>(value);
		else
			throw std::runtime_error("cannot set type \""+type+"\"");
	}

	std::any parse_get(std::string path, const void* p, std::string type)
	{
		auto a = path.find("->");
		auto b = path.find(".");
		auto c = path.find("[");
		auto min = std::min(a, std::min(b, c));

		std::string first = path.substr(0, min);

		VkReflectInfo info = struct_reflection_map[type].members[first];
		if(info.name != first)
			throw std::runtime_error("cannot find member \""+first+"\" for type \""+type+"\"");

		const void* op = ((const uint8_t*)p) + info.offset;
		if(min == std::string::npos)
		{
			if(!is_primitive(info.type))
				throw std::runtime_error("cannot return non-primitive and non-enum type \""+info.type+"\"");
			return create_type(op, info.type);
		}
		else if(a < b && a < c)
		{
			if(!info.pointer)
				throw std::runtime_error("path refered to non-pointer member \""+info.name+"\" of type \""+type+"\"with '->'");

			const void* np = *((const void**)op);
			std::string rest = path.substr(min+2);
			return parse_get(rest, np, info.type);
		}
		else if(b < a && b < c)
		{
			if(info.pointer)
				throw std::runtime_error("path refered to pointer member \""+info.name+"\" of type \""+type+"\" with '.'");

			std::string rest = path.substr(min+1);
			return parse_get(rest, op, info.type);
		}
		else if(c < a && c < b)
		{
			if(!info.array)
				throw std::runtime_error("path refered to non-array member with \""+info.name+"\" of type \""+type+"\" with '[i]'");
			auto e = path.find("]", c);

			int i = std::stoi(path.substr(c+1, e-c-1));
			int length = std::any_cast<uint32_t>(parse_get(info.arrayLength, p, type));

			if(i >= length)
				throw std::runtime_error("array index "+std::to_string(i)+" for member \""+info.name+"\" exceeds its length of "
					+std::to_string(length)+" which can be found in member \""+info.arrayLength+"\"");

			const void* np = *((const void**)op);
			const void* ap = ((uint8_t*)np) + i * struct_reflection_map[info.type].size;
			std::string rest = path.substr(e+2);
			return parse_get(rest, ap, info.type);
		}
		else std::abort();
	}

	std::string parse_get_type(std::string path, std::string type)
	{
		auto a = path.find("->");
		auto b = path.find(".");
		auto c = path.find("[");
		auto min = std::min(a, std::min(b, c));

		std::string first = path.substr(0, min);

		VkReflectInfo info = struct_reflection_map[type].members[first];
		if(info.name != first)
			throw std::runtime_error("cannot find member \""+first+"\" for type \""+type+"\"");

		if(min == std::string::npos)
		{
			return info.type;
		}
		else if(a < b && a < c)
		{
			if(!info.pointer)
				throw std::runtime_error("path refered to non-pointer member \""+info.name+"\" of type \""+type+"\"with '->'");

			std::string rest = path.substr(min+2);
			return parse_get_type(rest, info.type);
		}
		else if(b < a && b < c)
		{
			if(info.pointer)
				throw std::runtime_error("path refered to pointer member \""+info.name+"\" of type \""+type+"\" with '.'");

			std::string rest = path.substr(min+1);
			return parse_get_type(rest, info.type);
		}
		else if(c < a && c < b)
		{
			if(!info.array)
				throw std::runtime_error("path refered to non-array member with \""+info.name+"\" of type \""+type+"\" with '[i]'");
			auto e = path.find("]", c);

			std::string rest = path.substr(e+2);
			return parse_get_type(rest, info.type);
		}
		else std::abort();
	}

	void parse_set(std::string path, void* p, std::string type, std::any value)
	{
		auto a = path.find("->");
		auto b = path.find(".");
		auto c = path.find("[");
		auto min = std::min(a, std::min(b, c));

		std::string first = path.substr(0, min);

		VkReflectInfo info = struct_reflection_map[type].members[first];
		if(info.name != first)
			throw std::runtime_error("cannot find member \""+first+"\" for type \""+type+"\"");

		void* op = ((uint8_t*)p) + info.offset;
		if(min == std::string::npos)
		{
			if(!is_primitive(info.type))
				throw std::runtime_error("cannot assign non-primitive and non-enum type \""+info.type+"\"");
			set_type(op, info.type, value);
		}
		else if(a < b && a < c)
		{
			if(!info.pointer)
				throw std::runtime_error("path refered to non-pointer member \""+info.name+"\" of type \""+type+"\"with '->'");

			void* np = *((void**)op);
			std::string rest = path.substr(min+2);
			parse_set(rest, np, info.type, value);
		}
		else if(b < a && b < c)
		{
			if(info.pointer)
				throw std::runtime_error("path refered to pointer member \""+info.name+"\" of type \""+type+"\" with '.'");

			std::string rest = path.substr(min+1);
			parse_set(rest, op, info.type, value);
		}
		else if(c < a && c < b)
		{
			if(!info.array)
				throw std::runtime_error("path refered to non-array member with \""+info.name+"\" of type \""+type+"\" with '[i]'");
			auto e = path.find("]", c);

			int i = std::stoi(path.substr(c+1, e-c-1));
			int length = std::any_cast<uint32_t>(parse_get(info.arrayLength, p, type));

			if(i >= length)
				throw std::runtime_error("array index "+std::to_string(i)+" for member \""+info.name+"\" exceeds its length of "
					+std::to_string(length)+" which can be found in member \""+info.arrayLength+"\"");

			void* np = *((void**)op);
			void* ap = ((uint8_t*)np) + i * struct_reflection_map[info.type].size;
			std::string rest = path.substr(e+2);
			return parse_set(rest, ap, info.type, value);
		}
		else std::abort();
	}

	std::any convert(std::any in, std::string type);
	std::any parse_rvalue(std::string expression, const void* p, std::string dtype)
	{
		try
		{
			size_t index;
			long l = std::stol(expression, &index, 10);
			return l;
		}
		catch (const std::invalid_argument& ex) {}
		try
		{
			size_t index;
			double d = std::stod(expression, &index);
			return d;
		}
		catch (const std::invalid_argument& ex) {}

		if(dtype=="VkBool32")
		{
			if(expression=="VK_TRUE")
				return (VkBool32) VK_TRUE;
			if(expression=="VK_FALSE")
				return (VkBool32) VK_FALSE;
		}

		if(enum_reflection_map.contains(dtype))
		{
			auto& submap = enum_reflection_map[dtype];
			if(submap.contains(expression))
			{
				return submap[expression].value;
			}
		}

		std::string flagBits = dtype.substr(0, dtype.size()-1)+"Bits";
		if(dtype.ends_with("Flags") && enum_reflection_map.contains(flagBits))
		{
			auto& submap = enum_reflection_map[flagBits];
			std::istringstream iss(expression);
			std::string elem;

			uint32_t flag = 0;
			while(std::getline(iss, elem, '|'))
			{
				elem.erase(std::remove_if(elem.begin(), elem.end(), [](char c){return isspace(c);}), elem.end());
				if(submap.contains(elem))
				{
					flag |= submap[elem].value;
				}
				else
				{
					flag |= std::any_cast<uint32_t>(convert(parse_rvalue(elem, nullptr, dtype), dtype));
				}
			}
			return flag;
		}

		throw std::runtime_error("cannot parse expression \""+expression+"\" for type \""+dtype+"\" or \""+flagBits+"\"");
	}

	std::any convert(std::any in, std::string type)
	{
		if(in.type() == typeid(long))
		{
			if(type=="uint32_t" || type=="VkBool32" || type.ends_with("Flags"))
				return (uint32_t)std::any_cast<long>(in);
			if(type=="float")
				return (float)std::any_cast<long>(in);
		}
		if(in.type() == typeid(uint32_t))
		{
			if(type=="uint32_t" || type=="VkBool32" || type.ends_with("Flags"))
				return in;
			if(enum_reflection_map.contains(type)) // all enums are uint32_t by default I hope
				return in;
		}
		if(in.type() == typeid(double))
		{
			if(type=="float")
				return (float)std::any_cast<double>(in);
		}

		throw std::runtime_error("cannot convert from type \""+std::string(in.type().name())+"\" to type \""+type+"\"");
	}

	void parse_assign(std::string expression, void* p, std::string type)
	{
		auto delim = expression.find("=");
		std::string left = expression.substr(0, delim);
		std::string right = expression.substr(delim+1);

		left.erase(std::remove_if(left.begin(), left.end(), isspace), left.end());
		right.erase(std::remove_if(right.begin(), right.end(), isspace), right.end());

		std::string ltype = parse_get_type(left, type);

		std::any rvalue = parse_rvalue(right, p, ltype);
		std::any converted = convert(rvalue, ltype);
		parse_set(left, p, type, converted);
	}

	std::string enum_to_string(uint32_t value, std::string type)
	{
		auto& a = enum_reflection_map[type];
		for(auto [k, v] : a)
		{
			if(v.value == value)
				return v.name;
		}

		return "unknown " + type + " " + std::to_string(value);
	}

	std::string parse_get_string(std::string path, const void* p, std::string type)
	{
		std::string rtype = parse_get_type(path, type);
		std::any value = parse_get(path, p, type);

		if(rtype=="uint32_t")
			return std::to_string(std::any_cast<uint32_t>(value));
		if(rtype=="int32_t")
			return std::to_string(std::any_cast<int32_t>(value));
		if(rtype=="VkBool32")
			return std::any_cast<VkBool32>(value) == VK_TRUE ? "VK_TRUE" : "VK_FALSE";
		if(enum_reflection_map.contains(rtype))
			return enum_to_string(std::any_cast<uint32_t>(value), rtype);

		throw std::runtime_error("cannot make a string out of type \""+rtype+"\"");
	}
}}
