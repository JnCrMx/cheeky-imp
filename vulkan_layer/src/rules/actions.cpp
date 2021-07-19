#include "rules/actions.hpp"
#include "logger.hpp"
#include "layer.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"

#include <istream>
#include <iterator>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <regex>

namespace CheekyLayer
{
	action_register<mark_action> mark_action::reg("mark");
	action_register<unmark_action> unmark_action::reg("unmark");
	action_register<verbose_action> verbose_action::reg("verbose");
	action_register<sequence_action> sequence_action::reg("seq");
	action_register<each_action> each_action::reg("each");
	action_register<on_action> on_action::reg("on");
	action_register<disable_action> disable_action::reg("disable");
	action_register<cancel_action> cancel_action::reg("cancel");
	action_register<log_action> log_action::reg("log");
	action_register<override_action> override_action::reg("override");

	void mark_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule&)
	{
		rule_env.marks[(VkHandle)handle].push_back(m_mark);
		ctx.logger << " Marked " << to_string(type) << " " << handle << " as \"" << m_mark << "\" ";
	}

	void mark_action::read(std::istream& in)
	{
		std::getline(in, m_mark, ')');
	}

	std::ostream& mark_action::print(std::ostream& out)
	{
		out << "mark(" << m_mark << ")";
		return out;
	}

	void unmark_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule&)
	{
		if(m_clear)
		{
			rule_env.marks[(VkHandle)handle].clear();
			ctx.logger << " Cleared marks of " << to_string(type) << " " << handle;
		}
		else
		{
			auto& marks = rule_env.marks[(VkHandle)handle];
			auto p = std::find(marks.begin(), marks.end(), m_mark);
			if(p != marks.end())
			{
				marks.erase(p);
				ctx.logger << " Unmarked " << to_string(type) << " " << handle << " as \"" << m_mark << "\" ";
			}
		}
	}

	void unmark_action::read(std::istream& in)
	{
		std::getline(in, m_mark, ')');
		if(m_mark == "*")
			m_clear = true;
	}

	std::ostream& unmark_action::print(std::ostream& out)
	{
		out << "unmark(" << (m_clear ? "*" : m_mark) << ")";
		return out;
	}

	void verbose_action::execute(selector_type, VkHandle, local_context& ctx, rule&)
	{
		if(ctx.printVerbose.has_value())
			ctx.printVerbose.value()(ctx.logger);
	}

	void verbose_action::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	std::ostream& verbose_action::print(std::ostream& out)
	{
		out << "verbose()";
		return out;
	}

	void sequence_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		for(auto& a : m_actions)
		{
			a->execute(type, handle, ctx, rule);
		}
	}

	void sequence_action::read(std::istream& in)
	{
		while(in.peek() != ')')
		{
			m_actions.push_back(read_action(in, m_type));

			skip_ws(in);
			char seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}

		check_stream(in, ')');
	}

	std::ostream& sequence_action::print(std::ostream &out)
	{
		out << "seq(";
		for(int i=0; i<m_actions.size(); i++)
		{
			m_actions[i]->print(out);
			out << (i==m_actions.size()-1 ? ")" : ", ");
		}
		return out;
	}

	void each_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		std::vector<VkHandle> handles;
		if(type == selector_type::Draw)
		{
			switch(m_selector->get_type())
			{
				case selector_type::Image:
					std::copy(ctx.info->draw.images.begin(), ctx.info->draw.images.end(), std::back_inserter(handles));
					break;
				case selector_type::Shader:
					std::copy(ctx.info->draw.shaders.begin(), ctx.info->draw.shaders.end(), std::back_inserter(handles));
					break;
				case selector_type::Buffer:
					std::copy(ctx.info->draw.vertexBuffers.begin(), ctx.info->draw.vertexBuffers.end(), std::back_inserter(handles));
					handles.push_back(ctx.info->draw.indexBuffer);
					break;
				default:
					throw std::runtime_error("unsupported selector type: "+std::to_string(m_selector->get_type()));
			}
		}
		for(auto h : handles)
		{
			if(m_selector->test(m_selector->get_type(), h, ctx))
				m_action->execute(m_selector->get_type(), h, ctx, rule);
		}
	}

	void each_action::read(std::istream& in)
	{
		m_selector = std::make_unique<selector>();
		in >> *m_selector;

		selector_type type = m_selector->get_type();
		if(type != selector_type::Image && type != selector_type::Buffer && type != selector_type::Shader)
			throw std::runtime_error("unsupported selector type "+to_string(type)+" only image, buffer and shader selectors are supported");
		
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		m_action = read_action(in, type);
		check_stream(in, ')');
	}

	std::ostream& each_action::print(std::ostream& out)
	{
		out << "each(";
		m_selector->print(out);
		out << ", ";
		m_action->print(out);
		out << ")";
		return out;
	}

	void on_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(!ctx.commandBuffer)
		{
			ctx.logger << " Cannot schedule action, because CommandBuffer is not known. We will execute it right now instead.";
			m_action->execute(type, handle, ctx, rule);
			return;
		}

		switch(m_event)
		{
			case EndCommandBuffer:
				rule_env.onEndCommandBuffer(ctx.commandBuffer, [this, type, handle, &rule](local_context& ctx2){
					this->m_action->execute(type, handle, ctx2, rule);
				});
				break;
			case QueueSubmit:
				rule_env.onQueueSubmit(ctx.commandBuffer, [this, type, handle, &rule](local_context& ctx2){
					this->m_action->execute(type, handle, ctx2, rule);
				});
				break;
		}
	}

	void on_action::read(std::istream& in)
	{
		std::string event;
		std::getline(in, event, ',');
		m_event = on_action_event_from_string(event);

		skip_ws(in);

		m_action = read_action(in, m_type);
		check_stream(in, ')');
	}

	std::ostream& on_action::print(std::ostream& out)
	{
		out << "on(" << on_action_event_to_string(m_event) << ", ";
		m_action->print(out);
		out << ")";
		return out;
	}

	void disable_action::execute(selector_type, VkHandle, local_context&, rule& rule)
	{
		rule.disable();
	}

	void disable_action::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	std::ostream& disable_action::print(std::ostream& out)
	{
		out << "disable()";
		return out;
	}

	void cancel_action::execute(selector_type, VkHandle, local_context& ctx, rule&)
	{
		ctx.canceled = true;
	}

	void cancel_action::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	std::ostream& cancel_action::print(std::ostream& out)
	{
		out << "cancel()";
		return out;
	}

	void log_action::execute(selector_type stype, VkHandle handle, local_context& ctx, rule&)
	{
		std::string s = m_text;

		s = std::regex_replace(s, std::regex("\\$type"), to_string(stype));

		{
			std::stringstream oss;
			oss << handle;
			s = std::regex_replace(s, std::regex("\\$handle"), oss.str());
		}

		{
			auto p = rule_env.marks.find(handle);
			if(p != rule_env.marks.find(handle))
			{
				auto& v = p->second;

				std::stringstream oss;
				oss << "[";
				for(int i=0; i<v.size(); i++)
				{
					s = std::regex_replace(s, std::regex("\\$marks\\["+std::to_string(i)+"\\]"), v[i]);
					oss << v[i] << (i == v.size()-1 ? "]" : ",");
				}
				s = std::regex_replace(s, std::regex("\\$marks\\[\\*\\]"), oss.str());
			}
		}

		ctx.logger << s;
	}

	void log_action::read(std::istream& in)
	{
		std::getline(in, m_text, ')');
		m_text = std::regex_replace(m_text, std::regex("\\$\\]"), ")");
		m_text = std::regex_replace(m_text, std::regex("\\$\\["), "(");
	}

	std::ostream& log_action::print(std::ostream& out)
	{
		std::string s = m_text;
		s = std::regex_replace(s, std::regex("\\)"), "$]");
		s = std::regex_replace(s, std::regex("\\("), "$[");

		out << "log(" << s << ")";
		return out;
	}

	void override_action::execute(selector_type, VkHandle, local_context& ctx, rule &)
	{
		ctx.overrides.push_back(m_expression);
	}

	void override_action::read(std::istream& in)
	{
		std::getline(in, m_expression, ')');
	}

	std::ostream& override_action::print(std::ostream& out)
	{
		out << "override(" << m_expression << ")";
		return out;
	}
}
