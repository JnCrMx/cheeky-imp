#pragma once

#include <vector>

#include "rules.hpp"

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
			vkstruct_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_path;

			static data_register<vkstruct_data> reg;
	};
}
