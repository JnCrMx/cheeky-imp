#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"

#include <experimental/iterator>

namespace CheekyLayer::rules::datas
{
	data_register<global_data> global_data::reg("global");
	data_register<local_data> local_data::reg("local");

	void global_data::read(std::istream& in)
	{
		std::getline(in, m_name, ')');
	}

	bool global_data::supports(selector_type, data_type)
	{
		return true;
	}

	data_value global_data::get(selector_type, data_type type, VkHandle, local_context& ctx, rule &)
	{
		if(!rule_env.global_variables.contains(m_name))
		{
			std::ostringstream oss;
			oss << "no such global variable: " << m_name << ", the following are available: ";
			std::transform(rule_env.global_variables.begin(), rule_env.global_variables.end(), std::experimental::make_ostream_joiner(ctx.logger.raw(), ", "), [](const auto& a){
				return a.first;
			});

			throw RULE_ERROR(oss.str());
		}

		auto& data = rule_env.global_variables.at(m_name);

		bool okay = true;
		switch(type)
		{
			case String:
				okay = std::holds_alternative<std::string>(data);
				break;
			case Raw:
				okay = std::holds_alternative<std::vector<uint8_t>>(data);
				break;
			case Handle:
				okay = std::holds_alternative<VkHandle>(data);
				break;
			case Number:
				okay = std::holds_alternative<double>(data);
				break;
			case List:
				okay = std::holds_alternative<data_list>(data);
				break;
		}
		if(!okay)
			throw RULE_ERROR("requested type "+to_string(type)+" does not match type of current reduction");

		return data;
	}

	std::ostream& global_data::print(std::ostream& out)
	{
		return out << "global(" << m_name << ")";
	}

	void local_data::read(std::istream& in)
	{
		std::getline(in, m_name, ')');
	}

	bool local_data::supports(selector_type, data_type)
	{
		return true;
	}

	data_value local_data::get(selector_type, data_type type, VkHandle, local_context& ctx, rule &)
	{
		if(!ctx.local_variables.contains(m_name))
		{
			std::ostringstream oss;
			oss << "no such local variable: " << m_name << ", the following are available: ";
			std::transform(ctx.local_variables.begin(), ctx.local_variables.end(), std::experimental::make_ostream_joiner(ctx.logger.raw(), ", "), [](const auto& a){
				return a.first;
			});

			throw RULE_ERROR(oss.str());
		}

		auto& data = ctx.local_variables.at(m_name);

		bool okay = true;
		switch(type)
		{
			case String:
				okay = std::holds_alternative<std::string>(data);
				break;
			case Raw:
				okay = std::holds_alternative<std::vector<uint8_t>>(data);
				break;
			case Handle:
				okay = std::holds_alternative<VkHandle>(data);
				break;
			case Number:
				okay = std::holds_alternative<double>(data);
				break;
			case List:
				okay = std::holds_alternative<data_list>(data);
				break;
		}
		if(!okay)
			throw RULE_ERROR("requested type "+to_string(type)+" does not match type of current reduction");

		return data;
	}

	std::ostream& local_data::print(std::ostream& out)
	{
		return out << "local(" << m_name << ")";
	}
}
