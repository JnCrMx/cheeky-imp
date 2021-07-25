#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "reflection/reflectionparser.hpp"

#include <algorithm>
#include <any>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace CheekyLayer
{
	data_register<string_data> string_data::reg("string");
	data_register<concat_data> concat_data::reg("concat");
	data_register<vkstruct_data> vkstruct_data::reg("vkstruct");

	void string_data::read(std::istream& in)
	{
		std::getline(in, m_string, ')');
	}

	data_value string_data::get(selector_type, data_type type, VkHandle, local_context &, rule &)
	{
		switch(type)
		{
			case data_type::String:
				return m_string;
			case data_type::Raw: {
				std::vector<uint8_t> v(m_string.size());
				std::transform(m_string.begin(), m_string.end(), v.begin(), [](char a) {return (uint8_t)a;});
				return v; }
			default:
				throw std::runtime_error("cannot return data type "+to_string(type));
		}
	}

	bool string_data::supports(selector_type, data_type type)
	{
		return type == data_type::String || type == data_type::Raw;
	}

	std::ostream& string_data::print(std::ostream& out)
	{
		out << "string(" << m_string << ")";
		return out;
	}

	void concat_data::read(std::istream& in)
	{
		while(in.peek() != ')')
		{
			m_parts.push_back(read_data(in, m_type));

			skip_ws(in);
			char seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}

		check_stream(in, ')');
	}

	data_value concat_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(type == data_type::String)
		{
			std::string str = "";
			for(auto& p : m_parts)
			{
				str += std::get<std::string>(p->get(stype, type, handle, ctx, rule));
			}
			return str;
		}
		if(type == data_type::Raw)
		{
			std::vector<uint8_t> vs;

			for(auto& p : m_parts)
			{
				auto v = std::get<std::vector<uint8_t>>(p->get(stype, type, handle, ctx, rule));
				std::copy(v.begin(), v.end(), std::back_inserter(vs));
			}
			return vs;
		}
		throw std::runtime_error("cannot concatenate data type "+to_string(type));
	}

	bool concat_data::supports(selector_type stype, data_type type)
	{
		return (type == data_type::String || type == data_type::Raw) && 
			std::all_of(m_parts.begin(), m_parts.end(), [stype, type](std::unique_ptr<data>& p) {return p->supports(stype, type);});
	}

	std::ostream& concat_data::print(std::ostream& out)
	{
		out << "concat(";
		for(int i=0; i<m_parts.size(); i++)
		{
			m_parts[i]->print(out);
			if(i != m_parts.size()-1)
				out << ", ";
		}
		out << ")";
		return out;
	}

	void vkstruct_data::read(std::istream& in)
	{
		std::getline(in, m_path, ')');

		if(m_type == selector_type::Pipeline)
		{
			reflection::parse_get_type(m_path, "VkGraphicsPipelineCreateInfo");
		}
		else
			throw std::runtime_error("cannot work with selector type "+to_string(m_type));
	}

	data_value vkstruct_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(stype == selector_type::Pipeline)
		{
			const VkGraphicsPipelineCreateInfo* info = ctx.info->pipeline.info;
			switch(type)
			{
				case data_type::Number:
					return (double) std::any_cast<uint32_t>(reflection::parse_get(m_path, info, "VkGraphicsPipelineCreateInfo"));
				case data_type::String:
					return reflection::parse_get_string(m_path, info, "VkGraphicsPipelineCreateInfo");
				case data_type::Raw: {
					uint32_t r = std::any_cast<uint32_t>(reflection::parse_get(m_path, info, "VkGraphicsPipelineCreateInfo"));
					std::vector<uint8_t> v(sizeof(r));
					std::copy((uint8_t*)&r, (uint8_t*)(&r+1), v.begin());
					return v; }
				default:
					throw std::runtime_error("cannot return data type "+to_string(type));
			}
		}
		throw std::runtime_error("cannot work with selector type "+to_string(stype));
	}

	bool vkstruct_data::supports(selector_type, data_type type)
	{
		return type == data_type::Number || type == data_type::String;
	}

	std::ostream& vkstruct_data::print(std::ostream& out)
	{
		out << "vkstruct(" << m_path << ")";
		return out;
	}
}
