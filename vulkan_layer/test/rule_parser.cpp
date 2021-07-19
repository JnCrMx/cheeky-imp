#include <sstream>
#include <stdexcept>
#include <string>
#include <iostream>

#include "rules/rules.hpp"

int main(int argc, char* argv[])
{
	std::string line = argv[1];

	std::unique_ptr<CheekyLayer::rule> rule = std::make_unique<CheekyLayer::rule>();
	try
	{
		std::istringstream iss(line);
		iss >> *rule;
	}
	catch(const std::runtime_error ex)
	{
		std::cerr << "Failed to parse rule \"" << line << "\": " << ex.what() << std::endl;
		return 1;
	}

	std::ostringstream oss;
	rule->print(oss);

	if(oss.str() != line)
	{
		std::cerr << "Failed to recrated rule \"" << line << "\": output is \"" << oss.str() << "\"" << std::endl;
		return 1;
	}
	return 0;
}