#include "reflection/reflectionparser.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan_core.h>

namespace CheekyLayer { namespace reflection {
	bool is_primitive(std::string type)
	{
		if(type=="uint32_t")
			return true;
		if(type=="VkBool32")
			return true;
		if(enum_reflection_map.contains(type)) // all enums can be returned as an uint32_t
			return true;
		return false;
	}

	std::any create_type(const void* p, std::string type)
	{
		if(type=="uint32_t")
			return *((uint32_t*)p);
		if(type=="VkBool32")
			return *((VkBool32*)p);
		if(enum_reflection_map.contains(type)) // all enums can be returned as an uint32_t
			return *((uint32_t*)p);
		throw std::runtime_error("cannot create type \""+type+"\"");
	}

	void set_type(void* p, std::string type, std::any value)
	{
		if(type=="uint32_t")
			*((uint32_t*)p) = std::any_cast<uint32_t>(value);
		else if(type=="VkBool32")
			*((VkBool32*)p) = std::any_cast<VkBool32>(value);
		else if(enum_reflection_map.contains(type)) // all enums can be set as an uint32_t
			*((uint32_t*)p) = std::any_cast<uint32_t>(value);
		else
			throw std::runtime_error("cannot set type \""+type+"\"");
	}

	std::any parse_get(std::string path, const void* p, std::string type)
	{
		auto a = path.find("->");
		auto b = path.find(".");
		auto min = std::min(a, b);

		std::string first = path.substr(0, min);

		VkReflectInfo info = struct_reflection_map[type][first];
		if(info.name != first)
			throw std::runtime_error("cannot find member \""+first+"\" for type \""+type+"\"");
		
		const void* op = ((const uint8_t*)p) + info.offset;
		if(min == std::string::npos)
		{
			if(!is_primitive(info.type))
				throw std::runtime_error("cannot return non-primitive and non-enum type \""+info.type+"\"");
			return create_type(op, info.type);
		}
		else if(a < b)
		{
			if(!info.pointer)
				throw std::runtime_error("path refered to non-pointer member \""+info.name+"\" of type \""+type+"\"with '->'");

			const void* np = *((const void**)op);
			std::string rest = path.substr(min+2);
			return parse_get(rest, np, info.type);
		}
		else
		{
			if(info.pointer)
				throw std::runtime_error("path refered to pointer member \""+info.name+"\" of type \""+type+"\" with '.'");
			
			std::string rest = path.substr(min+1);
			return parse_get(rest, op, info.type);
		}
	}

	std::string parse_get_type(std::string path, std::string type)
	{
		auto a = path.find("->");
		auto b = path.find(".");
		auto min = std::min(a, b);

		std::string first = path.substr(0, min);

		VkReflectInfo info = struct_reflection_map[type][first];
		if(info.name != first)
			throw std::runtime_error("cannot find member \""+first+"\" for type \""+type+"\"");
		
		if(min == std::string::npos)
		{
			return info.type;
		}
		else if(a < b)
		{
			if(!info.pointer)
				throw std::runtime_error("path refered to non-pointer member \""+info.name+"\" of type \""+type+"\"with '->'");

			std::string rest = path.substr(min+2);
			return parse_get_type(rest, info.type);
		}
		else
		{
			if(info.pointer)
				throw std::runtime_error("path refered to pointer member \""+info.name+"\" of type \""+type+"\" with '.'");
			
			std::string rest = path.substr(min+1);
			return parse_get_type(rest, info.type);
		}
	}

	void parse_set(std::string path, void* p, std::string type, std::any value)
	{
		auto a = path.find("->");
		auto b = path.find(".");
		auto min = std::min(a, b);

		std::string first = path.substr(0, min);

		VkReflectInfo info = struct_reflection_map[type][first];
		if(info.name != first)
			throw std::runtime_error("cannot find member \""+first+"\" for type \""+type+"\"");
		
		void* op = ((uint8_t*)p) + info.offset;
		if(min == std::string::npos)
		{
			if(!is_primitive(info.type))
				throw std::runtime_error("cannot assign non-primitive and non-enum type \""+info.type+"\"");
			set_type(op, info.type, value);
		}
		else if(a < b)
		{
			if(!info.pointer)
				throw std::runtime_error("path refered to non-pointer member \""+info.name+"\" of type \""+type+"\"with '->'");

			void* np = *((void**)op);
			std::string rest = path.substr(min+2);
			parse_set(rest, np, info.type, value);
		}
		else
		{
			if(info.pointer)
				throw std::runtime_error("path refered to pointer member \""+info.name+"\" of type \""+type+"\" with '.'");
			
			std::string rest = path.substr(min+1);
			parse_set(rest, op, info.type, value);
		}
	}

	std::any parse_rvalue(std::string expression, const void* p, std::string type, std::string dtype)
	{
		try
		{
			size_t index;
			long l = std::stol(expression, &index, 10);
			return l;
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

		throw std::runtime_error("cannot parse expression \""+expression+"\" for type \""+dtype+"\"");
	}

	std::any convert(std::any in, std::string type)
	{
		if(in.type() == typeid(long))
		{
			if(type=="uint32_t" || type=="VkBool32")
				return (uint32_t)std::any_cast<long>(in);
			if(type=="float")
				return (float)std::any_cast<long>(in);
		}
		if(in.type() == typeid(uint32_t))
		{
			if(type=="uint32_t" || type=="VkBool32")
				return in;
			if(enum_reflection_map.contains(type)) // all enums are uint32_t by default I hope
				return in;
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

		std::any rvalue = parse_rvalue(right, p, type, ltype);
		std::any converted = convert(rvalue, ltype);
		parse_set(left, p, type, converted);
	}
}}
