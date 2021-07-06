#pragma once

#include "execution_env.hpp"

#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace CheekyLayer
{
	void check_stream(std::istream& in, char expected);

	enum selector_type : int
	{
		Image,
		Buffer,
		Shader,
		Draw
	};
	selector_type from_string(std::string);
	std::string to_string(selector_type);

	class selector_condition
	{
		public:
			virtual void read(std::istream&) = 0;
			virtual bool test(selector_type, void*, local_context&) = 0;
			virtual std::ostream& print(std::ostream& out)
			{
				out << "unkownCondition()";
				return out;
			};
	};

	template<typename T> std::unique_ptr<selector_condition> default_create_condition(selector_type stype)
	{
		return std::make_unique<T>();
	}

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
			bool test(selector_type, void*, local_context&);
			std::ostream& print(std::ostream& out);
		private:
			selector_type m_type;
			std::vector<std::unique_ptr<selector_condition>> m_conditions;
			friend std::istream& operator>>(std::istream&, selector&);
			friend std::istream& operator>>(std::istream&, rule&);
	};

	class action
	{
		public:
			virtual void read(std::istream&) = 0;
			virtual void execute(selector_type, void*, local_context&) = 0;
			virtual std::ostream& print(std::ostream& out)
			{
				out << "unkownAction()";
				return out;
			};
	};


	template<typename T> std::unique_ptr<action> default_create_action(selector_type stype)
	{
		return std::make_unique<T>();
	}

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

	class rule
	{
		public:
			void execute(selector_type type, void* handle, local_context& ctx);
			std::ostream& print(std::ostream& out);
		private:
			std::unique_ptr<selector> m_selector;
			std::unique_ptr<action> m_action;
			friend std::istream& operator>>(std::istream&, rule&);
	};

	std::istream& operator>>(std::istream&, rule&);
	std::istream& operator>>(std::istream&, selector&);

	void execute_rules(std::vector<std::unique_ptr<rule>>& rules, selector_type type, void* handle, local_context& ctx);
}
