#include <sstream>
#include <stdexcept>
#include <string>
#include <iostream>
#include <iomanip>

#include "rules/rules.hpp"
#include "rules/reader.hpp"

int main(int argc, char* argv[])
{
	std::string file = argv[1];

	std::ifstream ifs(file);
	CheekyLayer::rules::numbered_streambuf numberer{ifs};

	std::unique_ptr<CheekyLayer::rules::rule> rule = std::make_unique<CheekyLayer::rules::rule>();
	while(ifs.good())
	{
		try
		{
			ifs >> *rule;
		}
		catch(const std::runtime_error& ex)
		{
			std::cout << "Error at " << numberer.line() << ":" << numberer.col() << ":\n\t" << ex.what() << std::endl;
			return 2;
		}
		rule->print(std::cout);
		std::cout << std::endl;
	}
	return 0;
}