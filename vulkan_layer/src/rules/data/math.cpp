#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "utils.hpp"

#include <exprtk.hpp>
#include <iomanip>

namespace CheekyLayer::rules::datas
{
	data_register<math_data> math_data::reg("math");

	void math_data::read(std::istream& in)
	{
		in >> std::quoted(m_expression, '`');
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
		out << "math(" << std::quoted(m_expression, '`');
		for(auto & [name, ptr] : m_variables)
		{
			out << ", " << name << " => ";
			ptr->print(out);
		}
		out << ")";
		return out;
	}
}
