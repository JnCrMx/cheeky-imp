#pragma once

#include "execution_env.hpp"

#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <variant>

namespace CheekyLayer
{
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
		DeviceDestroy
	};
	selector_type from_string(std::string);
	std::string to_string(selector_type);

	class selector_condition
	{
		public:
			selector_condition(selector_type type) : m_type(type) {}
			virtual void read(std::istream&) = 0;
			virtual bool test(selector_type, VkHandle, local_context&) = 0;
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
    	typedef std::map<std::string, std::unique_ptr<selector_condition>(*)(selector_type stype)> map_type;

		public:
			static std::unique_ptr<selector_condition> make_unique_condition(std::string const& s, selector_type stype)
			{
	        	map_type::iterator it = getMap()->find(s);
    	    	if(it == getMap()->end())
        	    	throw std::runtime_error("unknown condition type '"+s+"' for selector type "+to_string(stype));
	        	return it->second(stype);
    		}

		protected:
    		static map_type * getMap() {
        		if(!map) { map = new map_type; } 
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
			bool test(selector_type, VkHandle, local_context&);
			std::ostream& print(std::ostream& out);
			selector_type get_type() { return m_type; };
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
			virtual void read(std::istream&) = 0;
			virtual void execute(selector_type, VkHandle, local_context&, rule&) = 0;
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
    	typedef std::map<std::string, std::unique_ptr<action>(*)(selector_type stype)> map_type;

		public:
			static std::unique_ptr<action> make_unique_action(std::string const& s, selector_type stype)
			{
	        	map_type::iterator it = getMap()->find(s);
    	    	if(it == getMap()->end())
        	    	throw std::runtime_error("unknown action type '"+s+"' for selector type "+to_string(stype));
	        	return it->second(stype);
    		}

		protected:
    		static map_type * getMap() {
        		if(!map) { map = new map_type; } 
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
		Number
	};
	std::string to_string(data_type);
	data_type data_type_from_string(std::string);

	using data_value = std::variant<std::string, std::vector<uint8_t>, VkHandle, double>;

	class data
	{
		public:
			data(selector_type type) : m_type(type) {}
			virtual void read(std::istream&) = 0;
			virtual data_value get(selector_type, data_type, VkHandle, local_context&, rule&) = 0;
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
    	typedef std::map<std::string, std::unique_ptr<data>(*)(selector_type stype)> map_type;

		public:
			static std::unique_ptr<data> make_unique_data(std::string const& s, selector_type stype)
			{
	        	map_type::iterator it = getMap()->find(s);
    	    	if(it == getMap()->end())
        	    	throw std::runtime_error("unknown data type '"+s+"' for selector type "+to_string(stype));
	        	return it->second(stype);
    		}

		protected:
    		static map_type * getMap() {
        		if(!map) { map = new map_type; } 
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
			void execute(selector_type type, VkHandle handle, local_context& ctx);
			std::ostream& print(std::ostream& out);
			void disable();
			selector_type get_type() {
				return m_selector->get_type();
			}
			bool is_enabled() {
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

	void execute_rules(std::vector<std::unique_ptr<rule>>& rules, selector_type type, VkHandle handle, local_context& ctx);

	inline void (*rule_disable_callback)(rule* rule);
}
