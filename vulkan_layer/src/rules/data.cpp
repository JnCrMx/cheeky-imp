#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "reflection/reflectionparser.hpp"
#include "utils.hpp"

#include <algorithm>
#include <any>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vulkan/vulkan_core.h>

namespace CheekyLayer::rules::datas
{
	data_register<string_data> string_data::reg("string");
	data_register<string_data> string_data::reg2("s");
	data_register<concat_data> concat_data::reg("concat");
	data_register<vkstruct_data> vkstruct_data::reg("vkstruct");
	data_register<received_data> received_data::reg("received");
	data_register<convert_data> convert_data::reg("convert");
	data_register<string_clean_data> string_clean_data::reg("strclean");

	void string_data::read(std::istream& in)
	{
		std::getline(in, m_string, ')');
	}

	data_value string_data::get(selector_type, data_type type, VkHandle, local_context &, rule &)
	{
		std::string s = m_string;
		replace(s, "$]", ")");
		replace(s, "$[", "(");
		replace(s, "\\n", "\n");
		switch(type)
		{
			case data_type::String:
				return s;
			case data_type::Raw: {
				std::vector<uint8_t> v(s.size());
				std::transform(s.begin(), s.end(), v.begin(), [](char a) {return (uint8_t)a;});
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

		switch(m_type)
		{
			case selector_type::Pipeline:
				reflection::parse_get_type(m_path, "VkGraphicsPipelineCreateInfo");
				break;
			case selector_type::Draw:
				reflection::parse_get_type(m_path, "VkCmdDrawIndexed");
				break;
			default:
				throw std::runtime_error("cannot work with selector type "+to_string(m_type));
		}
	}

	data_value vkstruct_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		const void* structData;
		std::string structType;
		switch(stype)
		{
			case selector_type::Pipeline:
				structData = ctx.info->pipeline.info;
				structType = "VkGraphicsPipelineCreateInfo";
				break;
			case selector_type::Draw: {
				auto v = ctx.info->draw.info;
				if(std::holds_alternative<const reflection::VkCmdDrawIndexed*>(v))
				{
					structData = std::get<const reflection::VkCmdDrawIndexed*>(v);
					structType = "VkCmdDrawIndexed";
				}
				else if(std::holds_alternative<const reflection::VkCmdDraw*>(v))
				{
					structData = std::get<const reflection::VkCmdDraw*>(v);
					structType = "VkCmdDraw";
				}
				break;
			}
			default:
				throw std::runtime_error("cannot work with selector type "+to_string(stype));
		}

		switch(type)
		{
			case data_type::Number:
				return (double) std::any_cast<uint32_t>(reflection::parse_get(m_path, structData, structType));
			case data_type::String:
				return reflection::parse_get_string(m_path, structData, structType);
			case data_type::Raw: {
				uint32_t r = std::any_cast<uint32_t>(reflection::parse_get(m_path, structData, structType));
				std::vector<uint8_t> v(sizeof(r));
				std::copy((uint8_t*)&r, (uint8_t*)(&r+1), v.begin());
				return v; }
			default:
				throw std::runtime_error("cannot return data type "+to_string(type));
		}
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

	void received_data::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	data_value received_data::get(selector_type, data_type type, VkHandle, local_context& ctx, rule &)
	{
		if(type != data_type::Raw)
			throw std::runtime_error("cannot return data type "+to_string(type));
		receive_info& info = ctx.info->receive;
		return std::vector<uint8_t>(info.buffer, info.buffer + info.size);
	}

	bool received_data::supports(selector_type, data_type type)
	{
		return type == data_type::Raw;
	}

	std::ostream& received_data::print(std::ostream& out)
	{
		out << "received()";
		return out;
	}

	void convert_data::read(std::istream& in)
	{
		std::string st;
		std::getline(in, st, ',');
		skip_ws(in);
		srcType = data_type_from_string(st);

		std::string dt;
		std::getline(in, dt, ',');
		skip_ws(in);
		dstType = data_type_from_string(dt);

		m_src = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_src->supports(m_type, srcType))
			throw std::runtime_error("data does not support source data type "+to_string(srcType));
	}

	data_value convert_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(type != dstType)
			throw std::runtime_error("cannot return data type "+to_string(type));

		data_value value = m_src->get(stype, srcType, handle, ctx, rule);
		if(srcType == dstType)
			return value;

		switch(srcType)
		{
			case data_type::String: {
				std::string str = std::get<std::string>(value);
				switch(dstType)
				{
					case data_type::Raw:
						return std::vector<uint8_t>(str.begin(), str.end());
					case data_type::Number:
						return std::stod(str);
					default:
						throw std::runtime_error("no known conversion from "+to_string(srcType)+" to "+to_string(dstType));
				}
			}
			case data_type::Raw: {
				std::vector<uint8_t> raw = std::get<std::vector<uint8_t>>(value);
				switch(dstType)
				{
					case data_type::String:
						return std::string(raw.begin(), raw.end());
					default:
						throw std::runtime_error("no known conversion from "+to_string(srcType)+" to "+to_string(dstType));
				}
			}
			case data_type::Number: {
				double d = std::get<double>(value);
				switch(dstType)
				{
					case data_type::String:
						return std::to_string(d);
					default:
						throw std::runtime_error("no known conversion from "+to_string(srcType)+" to "+to_string(dstType));
				}
			}
			default:
				throw std::runtime_error("no known conversion from "+to_string(srcType)+" to "+to_string(dstType));
		}
	}

	bool convert_data::supports(selector_type, data_type type)
	{
		return type == dstType;
	}

	std::ostream& convert_data::print(std::ostream& out)
	{
		out << "convert(" << to_string(srcType) << ", " << to_string(dstType) << ", ";
		m_src->print(out);
		out << ")";
		return out;
	}

	void string_clean_data::read(std::istream& in)
	{
		m_data = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_data->supports(m_type, data_type::String))
			throw std::runtime_error("data does not support string data");
	}

	data_value string_clean_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(type != data_type::String)
			throw std::runtime_error("cannot return data type "+to_string(type));
		std::string s = std::get<std::string>(m_data->get(stype, data_type::String, handle, ctx, rule));

		s.erase(std::remove_if(s.begin(), s.end(), [](char c) {
			return !std::isprint(c);
		}), s.end());
		return s;
	}

	bool string_clean_data::supports(selector_type, data_type type)
	{
		return type == data_type::String;
	}

	std::ostream& string_clean_data::print(std::ostream& out)
	{
		out << "strclean(";
		m_data->print(out);
		out << ")";
		return out;
	}
}
