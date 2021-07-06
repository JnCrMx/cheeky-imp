#include "rules/rules.hpp"
#include "logger.hpp"
#include "rules/execution_env.hpp"

#include <exception>
#include <istream>
#include <memory>
#include <stdexcept>
#include <string>

namespace CheekyLayer
{
	condition_factory::map_type* condition_factory::map = NULL;
	action_factory::map_type* action_factory::map = NULL;

	void skip_ws(std::istream& in)
	{
		while(in.peek()==' ')
			in.get();
	}

	void check_stream(std::istream &in, char expected)
	{
		char c = in.get();
		if(c != expected)
			throw std::runtime_error("expected '"+std::string(1, expected)+"' but got '"+std::string(1, c)+"' instead");
	}

	std::istream& operator>>(std::istream& in, rule& rule)
	{
		rule.m_selector = std::make_unique<selector>();
		in >> *rule.m_selector;

		std::string arrow;
		in >> arrow;
		if(arrow != "->")
			throw std::runtime_error("expected '->', but got '"+arrow+"' instead");
		skip_ws(in);

		std::string actionType;
		std::getline(in, actionType, '(');
		rule.m_action = action_factory::make_unique_action(actionType, rule.m_selector->m_type);
		rule.m_action->read(in);

		skip_ws(in);
		if(in.good())
			throw std::runtime_error("found characters after end of rule");

		return in;
	}

	std::istream& operator>>(std::istream& in, selector& selector)
	{
		std::string type;
		std::getline(in, type, '{');

		selector.m_type = from_string(type);

		while(in.peek() != '}')
		{
			std::string conditionType;
			if(!std::getline(in, conditionType, '('))
				throw std::runtime_error("cannot read condition type");
			std::unique_ptr<selector_condition> cptr = condition_factory::make_unique_condition(conditionType, selector.m_type);
			cptr->read(in);
			selector.m_conditions.push_back(std::move(cptr));

			skip_ws(in);
			char seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}

		check_stream(in, '}');

		return in;
	}

	selector_type from_string(std::string s)
	{
		if(s=="image")
			return selector_type::Image;
		if(s=="buffer")
			return selector_type::Buffer;
		if(s=="shader")
			return selector_type::Shader;
		if(s=="draw")
			return selector_type::Draw;
		throw std::runtime_error("unknown selector_type \""+s+"\"");
	}

	std::string to_string(selector_type type)
	{
		switch(type)
		{
			case selector_type::Image:
				return "image";
			case selector_type::Buffer:
				return "buffer";
			case selector_type::Shader:
				return "shader";
			case selector_type::Draw:
				return "draw";
			default:
				return "unknown" + std::to_string((int)type);
		}
	}

	bool selector::test(selector_type type, void *handle, local_context& ctx)
	{
		if(type != m_type)
			return false;
		for(auto& c : m_conditions)
		{
			if(!c->test(type, handle, ctx))
				return false;
		}
		return true;
	}

	void rule::execute(selector_type type, void* handle, local_context& ctx)
	{
		if(m_selector->test(type, handle, ctx))
			m_action->execute(type, handle, ctx);
	}
	void execute_rules(std::vector<std::unique_ptr<rule>>& rules, selector_type type, void* handle, local_context& ctx)
	{
		for(auto& r : rules)
		{
			try
			{
				r->execute(type, handle, ctx);
			}
			catch(std::runtime_error& ex)
			{
				ctx.logger << "Failed to execute a rule: " << ex.what();
			}
		}
	}

	std::ostream& rule::print(std::ostream &out)
	{
		m_selector->print(out);
		out << " -> ";
		m_action->print(out);
		return out;
	}

	std::ostream& selector::print(std::ostream &out)
	{
		out << to_string(m_type) << "{";
		for(int i=0; i<m_conditions.size(); i++)
		{
			m_conditions[i]->print(out);
			if(i != m_conditions.size() - 1)
				out << ", ";
		}
		out << "}";
		return out;
	}
}