#pragma once

#include "rules.hpp"
#include "rules/execution_env.hpp"
#include <memory>

namespace CheekyLayer
{
	class hash_condition : public selector_condition
	{
		public:
			virtual void read(std::istream&);
			virtual bool test(selector_type, void*, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_hash;

    		static condition_register<hash_condition> reg;
	};

	class mark_condition : public selector_condition
	{
		public:
			virtual void read(std::istream&);
			virtual bool test(selector_type, void*, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_mark;

    		static condition_register<mark_condition> reg;
	};

	class with_condition : public selector_condition
	{
		public:
			virtual void read(std::istream&);
			virtual bool test(selector_type, void*, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<selector> m_selector;

    		static condition_register<with_condition> reg;
	};
}