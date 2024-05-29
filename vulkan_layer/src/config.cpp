#include "config.hpp"

#include <sstream>

namespace CheekyLayer
{
	const std::function<bool(std::string)> config::to_bool = [](std::string s) {return s == "true";};

	config::config(const std::unordered_map<std::string, std::string>& v) : values(v)
	{
		log_file = map<std::filesystem::path>("logFile", [](std::string s) {return std::filesystem::path(s);});
		log_level = map<std::string>("logLevel", [](std::string s) {return s;});

		application = map<std::string>("application", [](std::string s) {return s;});
		hook_draw_calls = map<bool>("hookDraw", to_bool);
		rule_file = map<std::filesystem::path>("ruleFile", [](std::string s) {return std::filesystem::path(s);});

		dump = map<bool>("dump", to_bool);
		dump_png = map<bool>("dumpPng", to_bool);
		dump_png_flipped = map<bool>("dumpPngFlipped", to_bool);
		dump_directory = map<std::filesystem::path>("dumpDirectory", [](std::string s) {return std::filesystem::path(s);});

		override = map<bool>("override", to_bool);
		override_png_flipped = map<bool>("overridePngFlipped", to_bool);
		override_directory = map<std::filesystem::path>("overrideDirectory", [](std::string s) {return std::filesystem::path(s);});
	}

	const std::string& config::operator[](const std::string& key) const
	{
		return values.contains(key) ? values.at(key) : DEFAULT_CONFIG.values.at(key);
	}

	bool config::has(const std::string& key)
	{
		return values.contains(key);
	}

	std::istream& operator>>(std::istream& is, config& config)
	{
		std::unordered_map<std::string, std::string> values;

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
					values[key] = value;
				}
			}
		}
		config = CheekyLayer::config(values);

		return is;
	}

	const config DEFAULT_CONFIG(std::unordered_map<std::string, std::string>({
		{"dump", "false"},
		{"dumpPng", "false"},
		{"dumpPngFlipped", "false"},
		{"dumpDirectory", "/tmp/vulkan_dump"},
		{"override", "true"},
		{"overridePngFlipped", "false"},
		{"overrideDirectory", "./override"},
		{"logFile", "cheeky_layer.txt"},
		{"logLevel", "debug"},
		{"ruleFile", "rules.txt"},
		{"hookDraw", "false"},
		{"application", ""},
		{"pluginDirectory", "./plugins"}
	}));
}
