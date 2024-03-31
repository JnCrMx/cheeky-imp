#pragma once

#include "rules.hpp"
#include "rules/execution_env.hpp"
#include <memory>
#include <stdexcept>

namespace CheekyLayer::rules::conditions
{
	class hash_condition : public selector_condition
	{
		public:
			hash_condition(selector_type type) : selector_condition(type) {
				if(type != selector_type::Buffer && type != selector_type::Image && type != selector_type::Shader)
					throw std::runtime_error("the \"hash\" condition is only supported for buffer, image and shader selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual bool test(selector_type, VkHandle, global_context&, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_hash;

    		static condition_register<hash_condition> reg;
	};

	class mark_condition : public selector_condition
	{
		public:
			mark_condition(selector_type type) : selector_condition(type) {}
			virtual void read(std::istream&);
			virtual bool test(selector_type, VkHandle, global_context&, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_mark;

    		static condition_register<mark_condition> reg;
	};

	class with_condition : public selector_condition
	{
		public:
			with_condition(selector_type type) : selector_condition(type) {
				if(type != selector_type::Draw && type != selector_type::Pipeline)
					throw std::runtime_error("the \"with\" condition is only supported for draw and pipeline selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual bool test(selector_type, VkHandle, global_context&, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<selector> m_selector;

    		static condition_register<with_condition> reg;
	};

	class not_condition : public selector_condition
	{
		public:
			not_condition(selector_type type) : selector_condition(type) {}
			virtual void read(std::istream&);
			virtual bool test(selector_type, VkHandle, global_context&, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<selector_condition> m_condition;

    		static condition_register<not_condition> reg;
	};

	class or_condition : public selector_condition
	{
		public:
			or_condition(selector_type type) : selector_condition(type) {}
			virtual void read(std::istream&);
			virtual bool test(selector_type, VkHandle, global_context&, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::vector<std::unique_ptr<selector_condition>> m_conditions;

    		static condition_register<or_condition> reg;
	};

	class compare_condition : public selector_condition
	{
		enum comparison_operator
		{
			Equal,
			NotEqual,
			LessThan,
			LessThanOrEqual,
			GreaterThan,
			GreaterThanOrEqual
		};

		public:
			compare_condition(selector_type type) : selector_condition(type) {}
			virtual void read(std::istream&);
			virtual bool test(selector_type, VkHandle, global_context&, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<data> m_left;
			comparison_operator m_op;
			data_type m_dtype;
			std::unique_ptr<data> m_right;

			static std::string op_to_string(comparison_operator op);
			static comparison_operator op_from_string(std::string s);

    		static condition_register<compare_condition> reg;
	};

	class custom_condition : public selector_condition
	{
		public:
			custom_condition(selector_type type) : selector_condition(type) {
				if(type != selector_type::Custom)
					throw std::runtime_error("the \"custom\" condition is only supported for custom selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual bool test(selector_type, VkHandle, global_context&, local_context&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_tag;

    		static condition_register<custom_condition> reg;
	};
}
