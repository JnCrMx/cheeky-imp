#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "utils.hpp"

namespace CheekyLayer::rules::datas
{
	data_register<current_reduction_data> current_reduction_data::reg("current_reduction");
	data_register<reduce_data> reduce_data::reg("reduce");

	void current_reduction_data::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	data_value current_reduction_data::get(selector_type, data_type type, VkHandle, local_context& ctx, rule &)
	{
		if(!ctx.currentReduction)
			throw RULE_ERROR("no current reduction");

		bool okay = true;
		switch(type)
		{
			case String:
				okay = std::holds_alternative<std::string>(*ctx.currentReduction);
				break;
			case Raw:
				okay = std::holds_alternative<std::vector<uint8_t>>(*ctx.currentReduction);
				break;
			case Handle:
				okay = std::holds_alternative<VkHandle>(*ctx.currentReduction);
				break;
			case Number:
				okay = std::holds_alternative<double>(*ctx.currentReduction);
				break;
			case List:
				okay = std::holds_alternative<data_list>(*ctx.currentReduction);
				break;
		}
		if(!okay)
			throw RULE_ERROR("requested type "+to_string(type)+" does not match type of current reduction");

		return *ctx.currentReduction;
	}

	bool current_reduction_data::supports(selector_type, data_type)
	{
		return true;
	}

	std::ostream& current_reduction_data::print(std::ostream& out)
	{
		return out << "current_reduction()";
	}

	void reduce_data::read(std::istream& in)
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
			throw RULE_ERROR(of.str());
		}

		std::string dt;
		std::getline(in, dt, ',');
		skip_ws(in);
		m_dstType = data_type_from_string(dt);

		m_init = read_data(in, m_type);
		check_stream(in, ',');
		skip_ws(in);
		if(!m_init->supports(m_type, m_dstType))
		{
			std::ostringstream of;
			of << "Initial value \"";
			m_src->print(of);
			of << "\" does not support requested element type " << to_string(m_dstType);
			throw RULE_ERROR(of.str());
		}

		m_accumulator = read_data(in, m_type);
		check_stream(in, ')');
		if(!m_accumulator->supports(m_type, m_dstType))
		{
			std::ostringstream of;
			of << "Accumulator \"";
			m_src->print(of);
			of << "\" does not support requested element type " << to_string(m_dstType);
			throw RULE_ERROR(of.str());
		}
	}

	bool reduce_data::supports(selector_type, data_type t)
	{
		return t == m_dstType;
	}

	data_value reduce_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		data_list src = std::get<data_list>(m_src->get(stype, data_type::List, handle, ctx, rule));

		auto savedElem = ctx.currentElement;
		auto savedRedu = ctx.currentReduction;

		data_value val = m_init->get(stype, m_dstType, handle, ctx, rule);
		ctx.currentReduction = &val;
		for(data_value& elem : src.values)
		{
			ctx.currentElement = &elem;
			val = m_accumulator->get(stype, m_dstType, handle, ctx, rule);
		}
		ctx.currentReduction = savedRedu;
		ctx.currentElement = savedElem;

		return val;
	}

	std::ostream& reduce_data::print(std::ostream& out)
	{
		out << "reduce(";
		m_src->print(out);
		out << ", " << to_string(m_dstType) << ", ";
		m_init->print(out);
		out << ", ";
		m_accumulator->print(out);
		out << ")";
		return out;
	}
}
