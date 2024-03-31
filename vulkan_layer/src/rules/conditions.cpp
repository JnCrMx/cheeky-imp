#include "rules/conditions.hpp"
#include "rules/rules.hpp"
#include "rules/execution_env.hpp"

#include <compare>
#include <memory>
#include <ostream>
#include <string>
#include <istream>

namespace CheekyLayer::rules::conditions
{
	condition_register<hash_condition> hash_condition::reg("hash");
	condition_register<mark_condition> mark_condition::reg("mark");
	condition_register<with_condition> with_condition::reg("with");
	condition_register<not_condition> not_condition::reg("not");
	condition_register<or_condition> or_condition::reg("or");
	condition_register<compare_condition> compare_condition::reg("compare");
	condition_register<custom_condition> custom_condition::reg("custom");

	bool hash_condition::test(selector_type stype, VkHandle handle, global_context& global, local_context&)
	{
		auto p = global.hashes.find(handle);
		if(p == global.hashes.end())
			return false;
		return p->second == m_hash;
	}

	void hash_condition::read(std::istream& in)
	{
		std::getline(in, m_hash, ')');
	}

	std::ostream& hash_condition::print(std::ostream& out)
	{
		out << "hash(" << m_hash << ")";
		return out;
	}

	bool mark_condition::test(selector_type stype, VkHandle handle, global_context& global, local_context&)
	{
		auto p = global.marks.find(handle);
		if(p == global.marks.end())
			return false;
		return p->second.contains(m_mark);
	}

	void mark_condition::read(std::istream& in)
	{
		std::getline(in, m_mark, ')');
	}

	std::ostream& mark_condition::print(std::ostream& out)
	{
		out << "mark(" << m_mark << ")";
		return out;
	}

	bool with_condition::test(selector_type stype, VkHandle handle, global_context& global, local_context& local)
	{
		std::vector<std::pair<selector_type, VkHandle>> list;
		if(stype == selector_type::Draw)
		{
			auto& info = std::get<draw_info>(*local.info);
			std::transform(info.images.begin(), info.images.end(), std::back_inserter(list), [](auto h){
				return std::pair{selector_type::Image, h};
			});
			std::transform(info.vertexBuffers.begin(), info.vertexBuffers.end(), std::back_inserter(list), [](auto h){
				return std::pair{selector_type::Buffer, h};
			});
			std::transform(info.shaders.begin(), info.shaders.end(), std::back_inserter(list), [](auto h){
				return std::pair{selector_type::Shader, h};
			});
			list.push_back({selector_type::Buffer, info.indexBuffer});
		}
		if(stype == selector_type::Pipeline)
		{
			auto& info = std::get<pipeline_info>(*local.info);
			std::transform(info.shaderStages.begin(), info.shaderStages.end(), std::back_inserter(list), [](auto h){
				return std::pair{selector_type::Shader, h};
			});
		}

		for(auto [stype2, handle2] : list)
		{
			if(m_selector->test(stype2, handle2, global, local))
				return true;
		}

		return false;
	}

	void with_condition::read(std::istream& in)
	{
		m_selector = std::make_unique<selector>();
		in >> *m_selector;

		char end = in.get();
		if(end != ')')
			throw RULE_ERROR("expected ')', but got '"+std::string(1, end)+"' instead");
	}

	std::ostream& with_condition::print(std::ostream& out)
	{
		out << "with(";
		m_selector->print(out);
		out << ")";

		return out;
	}

	bool not_condition::test(selector_type type, VkHandle handle, global_context& global, local_context& local)
	{
		return !m_condition->test(type, handle, global, local);
	}

	void not_condition::read(std::istream& in)
	{
		m_condition = read_condition(in, m_type);
		skip_ws(in);
		check_stream(in, ')');
	}

	std::ostream& not_condition::print(std::ostream& out)
	{
		out << "not(";
		m_condition->print(out);
		out << ")";
		return out;
	}

	bool or_condition::test(selector_type type, VkHandle handle, global_context& global, local_context& local)
	{
		for(auto& c : m_conditions)
		{
			if(c->test(type, handle, global, local))
				return true;
		}
		return false;
	}

	void or_condition::read(std::istream& in)
	{
		while(in.peek() != ')')
		{
			m_conditions.push_back(read_condition(in, m_type));

			skip_ws(in);
			char seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}

		check_stream(in, ')');
	}

	std::ostream& or_condition::print(std::ostream& out)
	{
		out << "or(";
		for(int i=0; i<m_conditions.size(); i++)
		{
			m_conditions[i]->print(out);
			out << (i==m_conditions.size()-1 ? ")" : ", ");
		}
		return out;
	}

	bool compare_condition::test(selector_type type, VkHandle handle, global_context& global, local_context& local)
	{
		rule dummy{};
		data_value v1 = m_left->get(type, m_dtype, handle, global, local, dummy);
		data_value v2 = m_right->get(type, m_dtype, handle, global, local, dummy);

		std::partial_ordering result = std::partial_ordering::unordered;
		switch(m_dtype)
		{
			case data_type::Number:
				result = std::get<double>(v1) <=> std::get<double>(v2);
				break;
			case data_type::String:
				result = std::get<std::string>(v1) <=> std::get<std::string>(v2);
				break;
			case data_type::Raw:
				result = std::get<std::vector<uint8_t>>(v1) <=> std::get<std::vector<uint8_t>>(v2);
				break;
			default:
				break;
		}

		switch(m_op)
		{
			case comparison_operator::LessThan:
				return result == std::partial_ordering::less;
			case comparison_operator::LessThanOrEqual:
				return result == std::partial_ordering::less || result == std::partial_ordering::equivalent;
			case comparison_operator::Equal:
				return result == std::partial_ordering::equivalent;
			case comparison_operator::GreaterThanOrEqual:
				return result == std::partial_ordering::greater || result == std::partial_ordering::equivalent;
			case comparison_operator::GreaterThan:
				return result == std::partial_ordering::greater;
			case comparison_operator::NotEqual:
				return result == std::partial_ordering::greater || result == std::partial_ordering::less;
		}
		return false;
	}

	std::string compare_condition::op_to_string(comparison_operator op)
	{
		switch(op)
		{
			case LessThan:
				return "<";
			case LessThanOrEqual:
				return "<=";
			case Equal:
				return "==";
			case NotEqual:
				return "!=";
			case GreaterThanOrEqual:
				return ">=";
			case GreaterThan:
				return ">";
			default:
				return "unknown";
		}
	}

	compare_condition::comparison_operator compare_condition::op_from_string(std::string s)
	{
		if(s=="<" || s=="Less" || s=="L" || s=="LT" || s=="LessThan")
			return LessThan;
		if(s=="<=" || s=="=<" || s=="LessOrEqual" || s == "LE" || s=="LessThanOrEqual")
			return LessThanOrEqual;
		if(s=="==" || s=="=" || s=="Equal" || s=="EQ" || s=="EqualTo")
			return Equal;
		if(s==">=" || s=="=>" || s=="GreaterOrEqual" || s=="GE" || s=="GreaterThanOrEqual")
			return GreaterThanOrEqual;
		if(s==">" || s=="Greater" || s=="G" || s=="GT" || s=="GreaterThan")
			return GreaterThan;
		if(s=="!=" || s=="<>" || s=="NotEqual" || s=="NE" || s=="NotEqualTo")
			return NotEqual;
		throw RULE_ERROR("unknown comparison_operator \""+s+"\"");
	}

	void compare_condition::read(std::istream& in)
	{
		m_left = read_data(in, m_type);
		check_stream(in, ',');
		skip_ws(in);

		std::string op;
		std::getline(in, op, ',');
		m_op = op_from_string(op);
		skip_ws(in);

		m_right = read_data(in, m_type);
		check_stream(in, ')');

		if(m_left->supports(m_type, data_type::Number) && m_right->supports(m_type, data_type::Number))
			m_dtype = data_type::Number;
		else if(m_left->supports(m_type, data_type::String) && m_right->supports(m_type, data_type::String))
			m_dtype = data_type::String;
		else if(m_left->supports(m_type, data_type::Raw) && m_right->supports(m_type, data_type::Raw))
			m_dtype = data_type::Raw;
		else
			throw RULE_ERROR("only numbers and strings and raw data can be compared");
	}

	std::ostream& compare_condition::print(std::ostream& out)
	{
		out << "compare(";
		m_left->print(out);
		out << ", " << op_to_string(m_op) << ", ";
		m_right->print(out);
		out << ")";
		return out;
	}

	bool custom_condition::test(selector_type, VkHandle, global_context&, local_context& local)
	{
		return local.customTag == m_tag;
	}

	void custom_condition::read(std::istream& in)
	{
		std::getline(in, m_tag, ')');
	}

	std::ostream& custom_condition::print(std::ostream& out)
	{
		out << "custom(" << m_tag << ")";
		return out;
	}
}
