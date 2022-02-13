#pragma once

#include <vector>

#include "rules.hpp"
#include "rules/execution_env.hpp"

namespace CheekyLayer::rules::datas
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
				if(type != selector_type::Pipeline && type != selector_type::Draw)
					throw std::runtime_error("the \"vkstruct\" data is only supported for pipeline and draw selectors, but not for "+to_string(type)+" selectors");
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

	class number_data : public data
	{
		public:
			number_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			double m_number;

			static data_register<number_data> reg;
	};

	class math_data : public data
	{	
		public:
			math_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_expression;
			std::map<std::string, std::unique_ptr<data>> m_variables;

			static data_register<math_data> reg;
	};

	enum class raw_type
	{
		SInt8, SInt16, SInt32, SInt64,
		UInt8, UInt16, UInt32, UInt64,

		Float, Double
	};
	raw_type raw_type_from_string(std::string string);
	std::string to_string(raw_type type);

	class unpack_data : public data
	{

		public:
			unpack_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			raw_type m_rawType;
			size_t m_offset;
			std::unique_ptr<data> m_src;

			static data_register<unpack_data> reg;
	};

	class pack_data : public data
	{
		public:
			pack_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			raw_type m_rawType;
			std::unique_ptr<data> m_src;

			static data_register<pack_data> reg;
	};
}
