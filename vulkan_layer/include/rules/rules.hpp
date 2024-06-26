#pragma once

#include "execution_env.hpp"

#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <map>

#define BACKWARD_HAS_DW 1
#include <backward.hpp>

namespace CheekyLayer::rules
{
	class rule_error : public std::runtime_error
	{
		public:
			rule_error(std::string message) : std::runtime_error(message.c_str())
			{
				stack_trace.load_here();
				stack_trace.skip_n_firsts(1);
			}

			backward::StackTrace stack_trace{};
	};

	#define RULE_ERROR(message) (rule_error(std::string(__PRETTY_FUNCTION__)+": "+(message)))

	void skip_ws(std::istream& in);
	void check_stream(std::istream& in, char expected);

	enum selector_type : int
	{
		Image,
		Buffer,
		Shader,
		Draw,
		Pipeline,
		Init,
		Receive,
		DeviceCreate,
		DeviceDestroy,
		Present,
		SwapchainCreate,
		Custom
	};
	selector_type from_string(const std::string&);
	std::string to_string(selector_type);

	class selector_condition
	{
		public:
			selector_condition(selector_type type) : m_type(type) {}
			virtual ~selector_condition() = default;

			virtual void read(std::istream&) = 0;
			virtual bool test(selector_type, VkHandle, global_context&, local_context&) = 0;
			virtual std::ostream& print(std::ostream& out)
			{
				out << "unkownCondition()";
				return out;
			};
		protected:
			selector_type m_type;
	};

	template<typename T> std::unique_ptr<selector_condition> default_create_condition(selector_type stype)
	{
		return std::make_unique<T>(stype);
	}
	std::unique_ptr<selector_condition> read_condition(std::istream& in, selector_type type);

	struct condition_factory
	{
    	using map_type = std::map<std::string, std::unique_ptr<selector_condition>(*)(selector_type stype)>;

		public:
			static std::unique_ptr<selector_condition> make_unique_condition(std::string const& s, selector_type stype)
			{
	        	auto it = getMap()->find(s);
    	    	if(it == getMap()->end())
        	    	throw std::runtime_error("unknown condition type '"+s+"' for selector type "+to_string(stype));
	        	return it->second(stype);
    		}

		protected:
    		static map_type * getMap() {
        		if(map == nullptr) { map = new map_type; }
	        	return map;
    		}

		private:
    		static map_type * map;
	};

	template<typename T>
		struct condition_register : condition_factory {
    		condition_register(std::string const& s) {
        		getMap()->insert(std::make_pair(s, &default_create_condition<T>));
    	}
	};

	class rule;

	class selector
	{
		public:
			bool test(selector_type, VkHandle, global_context&, local_context&);
			std::ostream& print(std::ostream& out);
			[[nodiscard]] selector_type get_type() const { return m_type; };
		private:
			selector_type m_type;
			std::vector<std::unique_ptr<selector_condition>> m_conditions;
			friend std::istream& operator>>(std::istream&, selector&);
			friend std::istream& operator>>(std::istream&, rule&);
	};

	class rule;
	class action
	{
		public:
			action(selector_type type) : m_type(type) {}
			virtual ~action() = default;

			virtual void read(std::istream&) = 0;
			virtual void execute(selector_type, VkHandle, global_context&, local_context&, rule&) = 0;
			virtual std::ostream& print(std::ostream& out)
			{
				out << "unkownAction()";
				return out;
			};
		protected:
			selector_type m_type;
	};

	template<typename T> std::unique_ptr<action> default_create_action(selector_type stype)
	{
		return std::make_unique<T>(stype);
	}
	std::unique_ptr<action> read_action(std::istream& in, selector_type type);

	struct action_factory
	{
    	using map_type = std::map<std::string, std::unique_ptr<action>(*)(selector_type stype)>;

		public:
			static std::unique_ptr<action> make_unique_action(std::string const& s, selector_type stype)
			{
	        	auto it = getMap()->find(s);
    	    	if(it == getMap()->end())
        	    	throw std::runtime_error("unknown action type '"+s+"' for selector type "+to_string(stype));
	        	return it->second(stype);
    		}

		protected:
    		static map_type * getMap() {
        		if(map == nullptr) { map = new map_type; }
	        	return map;
    		}

		private:
    		static map_type * map;
	};

	template<typename T>
		struct action_register : action_factory {
    		action_register(std::string const& s) {
        		getMap()->insert(std::make_pair(s, &default_create_action<T>));
    	}
	};

	enum data_type : int
	{
		String,
		Raw,
		Handle,
		Number,
		List
	};
	std::string to_string(data_type);
	data_type data_type_from_string(const std::string&);

	struct data_list
	{
		data_list() = default;
		data_list(const std::initializer_list<data_value>&& v) : values(v) {}

		std::vector<data_value> values;
	};

	class data
	{
		public:
			data(selector_type type) : m_type(type) {}
			virtual ~data() = default;

			virtual void read(std::istream&) = 0;
			virtual data_value get(selector_type, data_type, VkHandle, global_context&, local_context&, rule&) = 0;
			virtual bool supports(selector_type, data_type) = 0;
			virtual std::ostream& print(std::ostream& out)
			{
				out << "unkownData()";
				return out;
			};
		protected:
			selector_type m_type;
	};

	template<typename T> std::unique_ptr<data> default_create_data(selector_type stype)
	{
		return std::make_unique<T>(stype);
	}
	std::unique_ptr<data> read_data(std::istream& in, selector_type type);

	struct data_factory
	{
    	using map_type = std::map<std::string, std::unique_ptr<data>(*)(selector_type stype)>;

		public:
			static std::unique_ptr<data> make_unique_data(std::string const& s, selector_type stype)
			{
	        	auto it = getMap()->find(s);
    	    	if(it == getMap()->end())
        	    	throw std::runtime_error("unknown data type '"+s+"' for selector type "+to_string(stype));
	        	return it->second(stype);
    		}

		protected:
    		static map_type * getMap() {
        		if(map == nullptr) { map = new map_type; }
	        	return map;
    		}

		private:
    		static map_type * map;
	};

	template<typename T>
		struct data_register : data_factory {
    		data_register(std::string const& s) {
        		getMap()->insert(std::make_pair(s, &default_create_data<T>));
    	}
	};

	class rule
	{
		public:
			void execute(selector_type type, VkHandle handle, global_context& global, local_context& local);
			std::ostream& print(std::ostream& out);
			void disable();
			[[nodiscard]] selector_type get_type() const {
				return m_selector->get_type();
			}
			[[nodiscard]] bool is_enabled() const {
				return !m_disabled;
			}
		private:
			std::unique_ptr<selector> m_selector;
			std::unique_ptr<action> m_action;
			bool m_disabled = false;
			friend std::istream& operator>>(std::istream&, rule&);
	};

	std::istream& operator>>(std::istream&, rule&);
	std::istream& operator>>(std::istream&, selector&);

	void execute_rules(std::vector<std::unique_ptr<rule>>& rules, selector_type type, VkHandle handle, global_context& global, local_context& local);

	inline void (*rule_disable_callback)(rule* rule);
}
