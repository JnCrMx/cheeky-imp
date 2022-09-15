#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "utils.hpp"

namespace CheekyLayer::rules::datas
{
	data_register<call_function_data> call_function_data::reg("call");

	void call_function_data::read(std::istream& in)
	{
		std::getline(in, m_function, ',');
		skip_ws(in);

		while(in.peek() != ')')
		{
			m_args.push_back(read_data(in, m_type));

			skip_ws(in);
			char seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}
		check_stream(in, ')');
	}

	std::ostream& call_function_data::print(std::ostream& out)
	{
		out << "call(" << m_function;
		for(auto& arg : m_args)
		{
			out << ", ";
			arg->print(out);
		}
		return out << ")";
	}

	bool call_function_data::supports(selector_type stype, data_type dtype)
	{
		if(!rule_env.user_functions.contains(m_function))
			return true;
		return rule_env.user_functions.at(m_function).data->supports(stype, dtype);
	}

	data_value call_function_data::get(selector_type stype, data_type dtype, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(!rule_env.user_functions.contains(m_function))
			throw RULE_ERROR("function not found: " + m_function);
		auto& func = rule_env.user_functions.at(m_function);
		if(func.arguments.size() - func.default_arguments.size() > m_args.size())
			throw RULE_ERROR("not enough arguments to call function " + m_function);

		struct restore {
			decltype(ctx)& ctx;
			decltype(ctx.local_variables) vars;

			restore(decltype(ctx)& ctx) : ctx(ctx), vars(ctx.local_variables) {}
			~restore() {ctx.local_variables = vars;}
		} restore = {ctx};

		int args = std::min(func.arguments.size(), m_args.size());
		for(int i=0; i<args; i++)
		{
			std::string argName = "_"+std::to_string(i+1);
			data_value value = m_args.at(i)->get(stype, func.arguments.at(i), handle, ctx, rule);
			ctx.local_variables[argName] = value;
		}
		for(int i=args; i<func.arguments.size(); i++)
		{
			std::string argName = "_"+std::to_string(i+1);
			int index = i-func.arguments.size()+func.default_arguments.size();
			data_value value = func.default_arguments.at(index)->get(stype, func.arguments.at(i), handle, ctx, rule);
			ctx.local_variables[argName] = value;
		}
		return func.data->get(stype, dtype, handle, ctx, rule);
	}
}
