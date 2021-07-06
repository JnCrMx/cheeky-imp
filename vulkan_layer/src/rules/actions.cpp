#include "rules/actions.hpp"
#include "logger.hpp"
#include "layer.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"

#include <istream>
#include <ostream>
#include <stdexcept>

namespace CheekyLayer
{
	action_register<mark_action> mark_action::reg("mark");
	action_register<verbose_action> verbose_action::reg("verbose");

	void mark_action::execute(selector_type type, void *handle, local_context& ctx)
	{
		rule_env.marks[handle] = m_mark;
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

	void verbose_action::execute(selector_type, void *, local_context& ctx)
	{
		if(ctx.printVerbose)
			ctx.printVerbose();
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
}
