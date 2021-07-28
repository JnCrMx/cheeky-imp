#pragma once

#include <vector>

#include "rules.hpp"
#include "rules/execution_env.hpp"

namespace CheekyLayer
{
	class string_data : public data
	{
		public:
			string_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_string;

			static data_register<string_data> reg;
			static data_register<string_data> reg2;
	};

	class concat_data : public data
	{
		public:
			concat_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::vector<std::unique_ptr<data>> m_parts;

			static data_register<concat_data> reg;
	};

	class vkstruct_data : public data
	{
		public:
			vkstruct_data(selector_type type) : data(type) {
				if(type != selector_type::Pipeline)
					throw std::runtime_error("the \"vkstruct\" data is only supported for pipeline selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_path;

			static data_register<vkstruct_data> reg;
	};

	class received_data : public data
	{
		public:
			received_data(selector_type type) : data(type) {
				if(type != selector_type::Receive)
					throw std::runtime_error("the \"received\" data is only supported for receive selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			static data_register<received_data> reg;
	};

	class convert_data : public data
	{
		public:
			convert_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<data> m_src;
			data_type srcType;
			data_type dstType;

			static data_register<convert_data> reg;
	};

	class string_clean_data : public data
	{
		public:
			string_clean_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<data> m_data;

			static data_register<string_clean_data> reg;
	};
}
