#pragma once

#include "rules.hpp"

namespace CheekyLayer
{
	class mark_action : public action
	{
		public:
			virtual void read(std::istream&);
			virtual void execute(selector_type, void*, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_mark;

			static action_register<mark_action> reg;
	};

	class verbose_action : public action
	{
		public:
			virtual void read(std::istream&);
			virtual void execute(selector_type, void*, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:

			static action_register<verbose_action> reg;
	};
}
