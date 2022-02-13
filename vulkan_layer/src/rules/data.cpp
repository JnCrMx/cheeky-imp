#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "reflection/reflectionparser.hpp"
#include "utils.hpp"

#include <algorithm>
#include <any>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vulkan/vulkan_core.h>
#include <exprtk.hpp>

namespace CheekyLayer::rules::datas
{
	data_register<string_data> string_data::reg("string");
	data_register<string_data> string_data::reg2("s");
	data_register<concat_data> concat_data::reg("concat");
	data_register<vkstruct_data> vkstruct_data::reg("vkstruct");
	data_register<received_data> received_data::reg("received");
	data_register<convert_data> convert_data::reg("convert");
	data_register<string_clean_data> string_clean_data::reg("strclean");
	data_register<number_data> number_data::reg("number");
	data_register<math_data> math_data::reg("math");
	data_register<unpack_data> unpack_data::reg("unpack");
	data_register<pack_data> pack_data::reg("pack");

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

	void number_data::read(std::istream& in)
	{
		in >> m_number;
		skip_ws(in);
		check_stream(in, ')');
	}

	data_value number_data::get(selector_type, data_type type, VkHandle, local_context &, rule &)
	{
		if(type != data_type::Number)
			throw std::runtime_error("cannot return data type "+to_string(type));

		return m_number;
	}

	bool number_data::supports(selector_type, data_type type)
	{
		return type == data_type::Number;
	}

	std::ostream& number_data::print(std::ostream& out)
	{
		out << "number(" << std::setprecision(10) << m_number << ")";
		return out;
	}

	void math_data::read(std::istream& in)
	{
		std::getline(in, m_expression, '\\');
		skip_ws(in);

		while(in.peek() == ',')
		{
			check_stream(in, ',');
			skip_ws(in);
			std::string name;
			in >> name;
			skip_ws(in);

			check_stream(in, '=');
			check_stream(in, '>');

			skip_ws(in);
			auto data = read_data(in, m_type);
			if(!data->supports(m_type, Number))
			{
				std::ostringstream of;
				of << "Variable \"";
				data->print(of);
				of << "\" does not support type Number.";
				throw std::runtime_error(of.str());
			}
			m_variables[name] = std::move(data);
		}
		check_stream(in, ')');

		exprtk::expression<double> expr;
		exprtk::symbol_table<double> table;

		double arg;
		for(auto& [name, ptr] : m_variables)
		{
			table.add_constant(name, arg);
		}
		table.add_constants();

		expr.register_symbol_table(table);

		exprtk::parser<double> parser;
		if(!parser.compile(m_expression, expr))
			throw std::runtime_error("Cannot parse expression: "+parser.error());
	}

	data_value math_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(type != data_type::Number)
			throw std::runtime_error("cannot return data type "+to_string(type));

		exprtk::expression<double> expr;
		exprtk::symbol_table<double> table;

		for(auto& [name, ptr] : m_variables)
		{
			double value = std::get<double>(ptr->get(stype, Number, handle, ctx, rule));
			table.add_constant(name, value);
		}
		table.add_constants();

		expr.register_symbol_table(table);

		exprtk::parser<double> parser;
		if(!parser.compile(m_expression, expr))
			throw std::runtime_error("Cannot parse expression: "+parser.error());

		return expr.value();
	}

	bool math_data::supports(selector_type, data_type type)
	{
		return type == Number;
	}

	std::ostream& math_data::print(std::ostream& out)
	{
		out << "math(" << m_expression << "\\";
		for(auto & [name, ptr] : m_variables)
		{
			out << ", " << name << " => ";
			ptr->print(out);
		}
		out << ")";
		return out;
	}

	raw_type from_string(std::string s)
	{
		if(s=="SInt8") return raw_type::SInt8;
		if(s=="SInt16") return raw_type::SInt16;
		if(s=="SInt32") return raw_type::SInt32;
		if(s=="SInt64") return raw_type::SInt64;

		if(s=="UInt8") return raw_type::UInt8;
		if(s=="UInt16") return raw_type::UInt16;
		if(s=="UInt32") return raw_type::UInt32;
		if(s=="UInt64") return raw_type::UInt64;

		if(s=="Float") return raw_type::Float;
		if(s=="Double") return raw_type::Double;

		throw std::runtime_error("unsupported raw_type: "+s);
	}

	std::string to_string(raw_type t)
	{
		switch(t)
		{
			case raw_type::SInt8: return "SInt8";
			case raw_type::SInt16: return "SInt16";
			case raw_type::SInt32: return "SInt32";
			case raw_type::SInt64: return "SInt64";

			case raw_type::UInt8: return "UInt8";
			case raw_type::UInt16: return "UInt16";
			case raw_type::UInt32: return "UInt32";
			case raw_type::UInt64: return "UInt64";

			case raw_type::Float: return "Float";
			case raw_type::Double: return "Double";
		}
	}

	void unpack_data::read(std::istream& in)
	{
		std::string type;
		std::getline(in, type, ',');
		m_rawType = from_string(type);
		skip_ws(in);

		in >> m_offset;
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		m_src = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_src->supports(m_type, data_type::Raw))
		{
			std::ostringstream of;
			of << "Source \"";
			m_src->print(of);
			of << "\" does not support type Raw.";
			throw std::runtime_error(of.str());
		}
	}

	data_value unpack_data::get(selector_type type, data_type dtype, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(dtype != data_type::Number)
			throw std::runtime_error("cannot return data type "+CheekyLayer::rules::to_string(dtype));

		std::vector<uint8_t> v = std::get<std::vector<uint8_t>>(m_src->get(type, data_type::Raw, handle, ctx, rule));

		size_t minSize;
		switch(m_rawType)
		{
			case raw_type::SInt8: minSize = sizeof(int8_t); break;
			case raw_type::SInt16: minSize = sizeof(int16_t); break;
			case raw_type::SInt32: minSize = sizeof(int32_t); break;
			case raw_type::SInt64: minSize = sizeof(int64_t); break;

			case raw_type::UInt8: minSize = sizeof(uint8_t); break;
			case raw_type::UInt16: minSize = sizeof(uint16_t); break;
			case raw_type::UInt32: minSize = sizeof(uint32_t); break;
			case raw_type::UInt64: minSize = sizeof(uint64_t); break;

			case raw_type::Float: minSize = sizeof(float); break;
			case raw_type::Double: minSize = sizeof(double); break;
		}
		if(v.size()-m_offset < minSize)
			throw std::runtime_error("Raw data of size "+std::to_string(v.size())+" is not big enough to contain number of type "
				+to_string(m_rawType)+" at offset "+std::to_string(m_offset));

		void* ptr = v.data() + m_offset;
		switch(m_rawType)
		{
			case raw_type::SInt8: return (double) (*((int8_t*)ptr));
			case raw_type::SInt16: return (double) (*((int16_t*)ptr));
			case raw_type::SInt32: return (double) (*((int32_t*)ptr));
			case raw_type::SInt64: return (double) (*((int64_t*)ptr));

			case raw_type::UInt8: return (double) (*((uint8_t*)ptr));
			case raw_type::UInt16: return (double) (*((uint16_t*)ptr));
			case raw_type::UInt32: return (double) (*((uint32_t*)ptr));
			case raw_type::UInt64: return (double) (*((uint64_t*)ptr));

			case raw_type::Float: return (double) (*((float*)ptr));
			case raw_type::Double: return (double) (*((double*)ptr));
		}
	}

	bool unpack_data::supports(selector_type, data_type type)
	{
		return type == Number;
	}

	std::ostream& unpack_data::print(std::ostream& out)
	{
		out << "unpack(" << to_string(m_rawType) << ", " << m_offset << ", ";
		m_src->print(out);
		out << ")";
		return out;
	}

	void pack_data::read(std::istream& in)
	{
		std::string type;
		std::getline(in, type, ',');
		m_rawType = from_string(type);
		skip_ws(in);

		m_src = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_src->supports(m_type, data_type::Number))
		{
			std::ostringstream of;
			of << "Source \"";
			m_src->print(of);
			of << "\" does not support type Number.";
			throw std::runtime_error(of.str());
		}
	}

	data_value pack_data::get(selector_type type, data_type dtype, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(dtype != data_type::Raw)
			throw std::runtime_error("cannot return data type "+CheekyLayer::rules::to_string(dtype));

		double d = std::get<double>(m_src->get(type, data_type::Number, handle, ctx, rule));

		size_t minSize;
		switch(m_rawType)
		{
			case raw_type::SInt8: minSize = sizeof(int8_t); break;
			case raw_type::SInt16: minSize = sizeof(int16_t); break;
			case raw_type::SInt32: minSize = sizeof(int32_t); break;
			case raw_type::SInt64: minSize = sizeof(int64_t); break;

			case raw_type::UInt8: minSize = sizeof(uint8_t); break;
			case raw_type::UInt16: minSize = sizeof(uint16_t); break;
			case raw_type::UInt32: minSize = sizeof(uint32_t); break;
			case raw_type::UInt64: minSize = sizeof(uint64_t); break;

			case raw_type::Float: minSize = sizeof(float); break;
			case raw_type::Double: minSize = sizeof(double); break;
		}

		std::vector<uint8_t> v(minSize);
		void* ptr = v.data();

		switch(m_rawType)
		{
			case raw_type::SInt8: (*(int8_t*)ptr) = (int8_t) d; break;
			case raw_type::SInt16: (*(int16_t*)ptr) = (int16_t) d; break;
			case raw_type::SInt32: (*(int32_t*)ptr) = (int32_t) d; break;
			case raw_type::SInt64: (*(int64_t*)ptr) = (int64_t) d; break;

			case raw_type::UInt8: (*(uint8_t*)ptr) = (uint8_t) d; break;
			case raw_type::UInt16: (*(uint16_t*)ptr) = (uint16_t) d; break;
			case raw_type::UInt32: (*(uint32_t*)ptr) = (uint32_t) d; break;
			case raw_type::UInt64: (*(uint64_t*)ptr) = (uint64_t) d; break;

			case raw_type::Float: (*(float*)ptr) = (float) d; break;
			case raw_type::Double: (*(double*)ptr) = (double) d; break;
		}
		return v;
	}

	bool pack_data::supports(selector_type, data_type type)
	{
		return type == Raw;
	}

	std::ostream& pack_data::print(std::ostream& out)
	{
		out << "pack(" << to_string(m_rawType) << ", ";
		m_src->print(out);
		out << ")";
		return out;
	}
}
