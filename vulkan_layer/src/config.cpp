#include "config.hpp"

#include <sstream>

namespace CheekyLayer
{
	const std::function<bool(std::string)> config::to_bool = [](std::string s) {return s == "true";};

	std::string config::operator[](std::string key)
	{
		return values[key];
	}

	bool config::has(std::string key)
	{
		return values.find(key) != values.end();
	}

	config config::operator+(config other)
	{
		config c;

		for(auto it = values.begin(); it != values.end(); it++)
			c.values[it->first] = it->second;
		
		for(auto it = other.values.begin(); it != other.values.end(); it++)
			c.values[it->first] = it->second;

		return c;
	}

	std::istream& operator>>(std::istream& is, config& config)
	{
		std::string line;
		while(std::getline(is, line))
		{
			std::istringstream is_line(line);
			std::string key;
			if(std::getline(is_line, key, '='))
			{
				std::string value;
				if (key[0] == '#')
					continue;

				if(std::getline(is_line, value))
				{
					config.values[key] = value;
				}
			}
		}

		return is;
	}

	const config DEFAULT_CONFIG(std::map<std::string, std::string>({
		{"dump", "false"},
		{"dumpDirectory", "/tmp/vulkan_dump"},
		{"override", "true"},
		{"overrideDirectory", "./override"},
		{"logFile", "cheeky_layer.txt"},
		{"ruleFile", "rules.txt"},
		{"hookDraw", "false"},
		{"application", ""},
		{"pluginDirectory", "./plugins"}
	}));
}