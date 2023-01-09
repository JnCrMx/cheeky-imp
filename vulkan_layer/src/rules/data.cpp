#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "reflection/reflectionparser.hpp"
#include "utils.hpp"

#include <iomanip>

namespace CheekyLayer::rules::datas
{
	data_register<string_data> string_data::reg("string");
	data_register<string_data> string_data::reg2("s");
	data_register<concat_data> concat_data::reg("concat");
	data_register<received_data> received_data::reg("received");
	data_register<string_clean_data> string_clean_data::reg("strclean");
	data_register<number_data> number_data::reg("number");
	data_register<split_data> split_data::reg("split");
	data_register<at_data> at_data::reg("at");

	void string_data::read(std::istream& in)
	{
		in >> std::quoted(m_string);
		check_stream(in, ')');
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
		out << "string(" << std::quoted(m_string) << ")";
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

	void split_data::read(std::istream& in)
	{
		m_data = read_data(in, m_type);
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		if(!m_data->supports(m_type, data_type::String))
			throw std::runtime_error("data does not support string data");

		in >> std::quoted(m_delimiter);
		skip_ws(in);
		check_stream(in, ')');
	}

	data_value split_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(type != data_type::List)
			throw std::runtime_error("cannot return data type "+to_string(type));
		std::string s = std::get<std::string>(m_data->get(stype, data_type::String, handle, ctx, rule));

		data_list list;
		std::string::size_type pos = 0;
		while((pos = s.find(m_delimiter)) != std::string::npos)
		{
			std::string token = s.substr(0, pos);
			list.values.push_back(token);
			s.erase(0, pos + m_delimiter.length());
		}
		list.values.push_back(s);

		return list;
	}

	bool split_data::supports(selector_type, data_type type)
	{
		return type == data_type::List;
	}

	std::ostream& split_data::print(std::ostream& out)
	{
		out << "split(";
		m_data->print(out);
		out << ", " << std::quoted(m_delimiter) << ")";
		return out;
	}

	void at_data::read(std::istream& in)
	{
		std::string i;
		std::getline(in, i, ',');
		std::istringstream{i} >> m_index;
		skip_ws(in);

		m_src = read_data(in, data::m_type);
		if(!m_src->supports(data::m_type, data_type::List))
		{
			std::ostringstream of;
			of << "Source \"";
			m_src->print(of);
			of << "\" does not support type List.";
			throw RULE_ERROR(of.str());
		}
		check_stream(in, ')');
	}

	data_value at_data::get(selector_type stype, data_type dtype, VkHandle handle, local_context& ctx, rule& rule)
	{
		auto list = std::get<data_list>(m_src->get(stype, data_type::List, handle, ctx, rule));
		auto& thing = list.values.at(m_index);

		bool okay = true;
		switch(dtype)
		{
			case String:
				okay = std::holds_alternative<std::string>(thing);
				break;
			case Raw:
				okay = std::holds_alternative<std::vector<uint8_t>>(thing);
				break;
			case Handle:
				okay = std::holds_alternative<VkHandle>(thing);
				break;
			case Number:
				okay = std::holds_alternative<double>(thing);
				break;
			case List:
				okay = std::holds_alternative<data_list>(thing);
				break;
		}
		if(!okay)
			throw RULE_ERROR("input data does not hold type \""+to_string(dtype)+"\" at index "+std::to_string(m_index));

		return thing;
	}

	bool at_data::supports(selector_type stype, data_type dtype)
	{
		return true;
	}

	std::ostream& at_data::print(std::ostream& out)
	{
		out << "at(" << m_index << ", ";
		m_src->print(out);
		return out << ")";
	}
}
