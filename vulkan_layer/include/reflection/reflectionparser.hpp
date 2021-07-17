#pragma once

#include <any>
#include "reflection/vkreflection.hpp"

namespace CheekyLayer { namespace reflection {
	std::any parse_get(std::string path, const void* p, std::string type);
	void parse_set(std::string path, void* p, std::string type, std::any value);

	void parse_assign(std::string expression, void* p, std::string type);
}}