#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "utils.hpp"

namespace CheekyLayer::rules::datas
{
	data_register<current_element_data> current_element_data::reg("current_element");
	data_register<current_index_data> current_index_data::reg("current_index");
	data_register<map_data> map_data::reg("map");
	
	void current_element_data::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	data_value current_element_data::get(selector_type, data_type type, VkHandle, local_context& ctx, rule &)
	{
		if(!ctx.currentElement)
			throw std::runtime_error("no current element");

		bool okay = true;
		switch(type)
		{
			case String:
				okay = std::holds_alternative<std::string>(*ctx.currentElement);
				break;
			case Raw:
				okay = std::holds_alternative<std::vector<uint8_t>>(*ctx.currentElement);
				break;
			case Handle:
				okay = std::holds_alternative<VkHandle>(*ctx.currentElement);
				break;
			case Number:
				okay = std::holds_alternative<double>(*ctx.currentElement);
				break;
			case List:
				okay = std::holds_alternative<data_list>(*ctx.currentElement);
				break;
		}
		if(!okay)
			throw std::runtime_error("requested type "+to_string(type)+" does not match type of current element");

		return *ctx.currentElement;
	}

	bool current_element_data::supports(selector_type, data_type)
	{
		return true;
	}

	std::ostream& current_element_data::print(std::ostream& out)
	{
		return out << "current_element()";
	}

	void current_index_data::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	data_value current_index_data::get(selector_type, data_type type, VkHandle, local_context& ctx, rule &)
	{
		return (double)ctx.currentIndex;
	}

	bool current_index_data::supports(selector_type, data_type type)
	{
		return type == data_type::Number;
	}

	std::ostream& current_index_data::print(std::ostream& out)
	{
		return out << "current_index()";
	}

	void map_data::read(std::istream& in)
	{
		m_src = read_data(in, m_type);
		check_stream(in, ',');
		skip_ws(in);
		if(!m_src->supports(m_type, data_type::List))
		{
			std::ostringstream of;
			of << "Source \"";
			m_src->print(of);
			of << "\" does not support type List.";
			throw std::runtime_error(of.str());
		}

		std::string dt;
		std::getline(in, dt, ',');
		skip_ws(in);
		m_elementDstType = data_type_from_string(dt);

		m_mapper = read_data(in, m_type);
		check_stream(in, ')');
		if(!m_mapper->supports(m_type, m_elementDstType))
		{
			std::ostringstream of;
			of << "Mapper \"";
			m_src->print(of);
			of << "\" does not support requested element type " << to_string(m_elementDstType);
			throw std::runtime_error(of.str());
		}
	}

	bool map_data::supports(selector_type, data_type t)
	{
		return t == data_type::List;
	}

	data_value map_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		data_list src = std::get<data_list>(m_src->get(stype, data_type::List, handle, ctx, rule));

		auto saved = ctx.currentElement;
		for(data_value& elem : src.values)
		{
			ctx.currentElement = &elem;
			elem = m_mapper->get(stype, m_elementDstType, handle, ctx, rule);
		}
		ctx.currentElement = saved;

		return src;
	}

	std::ostream& map_data::print(std::ostream& out)
	{
		out << "map(";
		m_src->print(out);
		out << ", " << to_string(m_elementDstType) << ", ";
		m_mapper->print(out);
		out << ")";
		return out;
	}
}
