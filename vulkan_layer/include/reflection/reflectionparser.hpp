#pragma once

#include <any>
#include "reflection/vkreflection.hpp"

namespace CheekyLayer { namespace reflection {
	std::any parse_get(std::string path, const void* p, std::string type);
	void parse_set(std::string path, void* p, std::string type, std::any value);
	std::string parse_get_type(std::string path, std::string type);

	void parse_assign(std::string expression, void* p, std::string type);

	std::string parse_get_string(std::string path, const void* p, std::string type);

	std::any parse_rvalue(std::string expression, const void* p, std::string dtype);
}}