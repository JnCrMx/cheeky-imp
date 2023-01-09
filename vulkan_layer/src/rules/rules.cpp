#include "rules/rules.hpp"
#include "logger.hpp"
#include "rules/execution_env.hpp"

#include <exception>
#include <istream>
#include <memory>
#include <stdexcept>
#include <string>

namespace CheekyLayer::rules
{
	condition_factory::map_type* condition_factory::map = nullptr;
	action_factory::map_type* action_factory::map = nullptr;
	data_factory::map_type* data_factory::map = nullptr;

	void skip_ws(std::istream& in)
	{
		while(std::isspace(in.peek()))
			(void)in.get();
	}

	void check_stream(std::istream &in, char expected)
	{
		skip_ws(in);
		int c = in.get();
		if(c != expected)
			throw std::runtime_error("expected '"+std::string(1, expected)+"' but got '"+std::string(1, static_cast<char>(c))+"' ("+std::to_string(c)+") instead");
	}

	std::unique_ptr<action> read_action(std::istream& in, selector_type type)
	{
		std::string actionType;
		if(!std::getline(in, actionType, '('))
			throw std::runtime_error("cannot read action type");
		skip_ws(in);
		std::unique_ptr<action> cptr = action_factory::make_unique_action(actionType, type);
		cptr->read(in);
		return cptr;
	}

	std::istream& operator>>(std::istream& in, rule& rule)
	{
		skip_ws(in);
		rule.m_selector = std::make_unique<selector>();
		in >> *rule.m_selector;
		skip_ws(in);

		std::string arrow;
		in >> arrow;
		if(arrow != "->")
			throw std::runtime_error("expected '->', but got '"+arrow+"' instead");
		skip_ws(in);

		rule.m_action = read_action(in, rule.m_selector->m_type);

		skip_ws(in);
		/*if(in.good())
			throw std::runtime_error("found characters after end of rule");*/

		return in;
	}

	std::unique_ptr<selector_condition> read_condition(std::istream& in, selector_type type)
	{
		std::string conditionType;
		if(!std::getline(in, conditionType, '('))
			throw std::runtime_error("cannot read condition type");
		skip_ws(in);
		std::unique_ptr<selector_condition> cptr = condition_factory::make_unique_condition(conditionType, type);
		cptr->read(in);
		return cptr;
	}

	std::unique_ptr<data> read_data(std::istream& in, selector_type type)
	{
		std::string dataType;
		if(!std::getline(in, dataType, '('))
			throw std::runtime_error("cannot read data type");
		skip_ws(in);
		std::unique_ptr<data> cptr = data_factory::make_unique_data(dataType, type);
		cptr->read(in);
		return cptr;
	}

	std::istream& operator>>(std::istream& in, selector& selector)
	{
		std::string type;
		std::getline(in, type, '{');

		selector.m_type = from_string(type);

		while(in.peek() != '}')
		{
			skip_ws(in);
			selector.m_conditions.push_back(read_condition(in, selector.m_type));

			skip_ws(in);
			int seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}

		check_stream(in, '}');

		return in;
	}

	selector_type from_string(const std::string& s)
	{
		if(s=="image")
			return selector_type::Image;
		if(s=="buffer")
			return selector_type::Buffer;
		if(s=="shader")
			return selector_type::Shader;
		if(s=="draw")
			return selector_type::Draw;
		if(s=="pipeline")
			return selector_type::Pipeline;
		if(s=="init")
			return selector_type::Init;
		if(s=="receive")
			return selector_type::Receive;
		if(s=="device_create")
			return selector_type::DeviceCreate;
		if(s=="device_destroy")
			return selector_type::DeviceDestroy;
		if(s=="present")
			return selector_type::Present;
		if(s=="swapchain_create")
			return selector_type::SwapchainCreate;
		if(s=="custom")
			return selector_type::Custom;
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
			case selector_type::Pipeline:
				return "pipeline";
			case selector_type::Init:
				return "init";
			case selector_type::Receive:
				return "receive";
			case selector_type::DeviceCreate:
				return "device_create";
			case selector_type::DeviceDestroy:
				return "device_destroy";
			case selector_type::Present:
				return "present";
			case selector_type::SwapchainCreate:
				return "swapchain_create";
			case selector_type::Custom:
				return "custom";
			default:
				return "unknown" + std::to_string((int)type);
		}
	}

	std::string to_string(data_type type)
	{
		switch(type)
		{
			case data_type::String:
				return "string";
			case data_type::Raw:
				return "raw";
			case data_type::Handle:
				return "handle";
			case data_type::Number:
				return "number";
			case data_type::List:
				return "list";
			default:
				return "unknown" + std::to_string((int)type);
		}
	}

	data_type data_type_from_string(const std::string& s)
	{
		if(s=="string")
			return data_type::String;
		if(s=="raw")
			return data_type::Raw;
		if(s=="handle")
			return data_type::Handle;
		if(s=="number")
			return data_type::Number;
		if(s=="list" || s=="vector" || s=="array")
			return data_type::List;
		throw std::runtime_error("unknown data_type \""+s+"\"");
	}

	bool selector::test(selector_type type, VkHandle handle, local_context& ctx)
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

	void rule::execute(selector_type type, VkHandle handle, local_context& ctx)
	{
		if(m_disabled)
			return;

		if(m_selector->test(type, handle, ctx))
			m_action->execute(type, handle, ctx, *this);
	}

	void rule::disable()
	{
		m_disabled = true;
		if(rule_disable_callback != nullptr)
		{
			rule_disable_callback(this);
		}
	}

	void execute_rules(std::vector<std::unique_ptr<rule>>& rules, selector_type type, VkHandle handle, local_context& ctx)
	{
		for(auto& r : rules)
		{
			try
			{
				r->execute(type, handle, ctx);
			}
			catch(const rule_error& ex)
			{
				ctx.logger << logger::error << "Failed to execute a rule: " << ex.what() << "\n";
				backward::Printer p;
				p.color_mode = backward::ColorMode::never;
				p.object = true;
				p.address = true;
				p.snippet = true;
				p.print(ex.stack_trace, ctx.logger.raw());
			}
			catch(const std::exception& ex)
			{
				ctx.logger << logger::error << "Failed to execute a rule: " << ex.what() << "(" << typeid(ex).name() << ")";
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
