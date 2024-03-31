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
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
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
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
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
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_path;

			static data_register<vkstruct_data> reg;
	};

	class vkdescriptor_data : public data
	{
		public:
			vkdescriptor_data(selector_type type) : data(type) {
				if(type != selector_type::Draw)
					throw std::runtime_error("the \"vkdescriptor\" data is only supported for draw selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			int m_set;
			int m_binding;
			int m_arrayIndex;

			static data_register<vkdescriptor_data> reg;
	};

	class received_data : public data
	{
		public:
			received_data(selector_type type) : data(type) {
				if(type != selector_type::Receive)
					throw std::runtime_error("the \"received\" data is only supported for receive selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
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
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
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
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
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
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
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
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
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

		Float, Double,

		Array
	};
	raw_type raw_type_from_string(std::string string);
	std::string to_string(raw_type type);

	class unpack_data : public data
	{

		public:
			unpack_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			raw_type m_rawType;
			size_t m_offset;
			int m_count = -1;
			std::unique_ptr<data> m_src;

			static data_register<unpack_data> reg;
	};

	class pack_data : public data
	{
		public:
			pack_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			raw_type m_rawType;
			std::unique_ptr<data> m_src;

			static data_register<pack_data> reg;
	};

	class current_element_data : public data
	{
		public:
			current_element_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			static data_register<current_element_data> reg;
	};

	class current_index_data : public data
	{
		public:
			current_index_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			static data_register<current_index_data> reg;
	};

	class map_data : public data
	{
		public:
			map_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<data> m_src;
			data_type m_elementDstType;
			std::unique_ptr<data> m_mapper;

			static data_register<map_data> reg;
	};

	class current_reduction_data : public data
	{
		public:
			current_reduction_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			static data_register<current_reduction_data> reg;
	};

	class reduce_data : public data
	{
		public:
			reduce_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<data> m_src;
			data_type m_dstType;
			std::unique_ptr<data> m_init;
			std::unique_ptr<data> m_accumulator;

			static data_register<reduce_data> reg;
	};

	class global_data : public data
	{
		public:
			global_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_name;

			static data_register<global_data> reg;
	};

	class local_data : public data
	{
		public:
			local_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_name;

			static data_register<local_data> reg;
	};

	class split_data : public data
	{
		public:
			split_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<data> m_data;
			std::string m_delimiter;

			static data_register<split_data> reg;
	};

	class call_function_data : public data
	{
		public:
			call_function_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_function;
			std::vector<std::unique_ptr<data>> m_args;

			static data_register<call_function_data> reg;
	};

	class at_data : public data
	{
		public:
			at_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			unsigned int m_index;
			std::unique_ptr<data> m_src;

			static data_register<at_data> reg;
	};

	class vkhandle_data : public data
	{
		public:
			vkhandle_data(selector_type type) : data(type) {}
			virtual void read(std::istream&);
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&);
			virtual bool supports(selector_type, data_type);
			virtual std::ostream& print(std::ostream&);
		private:
			static data_register<vkhandle_data> reg;
	};
}
