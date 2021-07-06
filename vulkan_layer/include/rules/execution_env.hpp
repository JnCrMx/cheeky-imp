#pragma once

#include "logger.hpp"
#include <map>
#include <string>
#include <vector>

namespace CheekyLayer
{
	enum selector_type : int;

	class global_context
	{
		public:
			std::map<void*, std::string> marks;
			std::map<void*, std::string> hashes;
	};

	inline global_context rule_env;

	struct draw_info
	{
		std::vector<void*>& images;
		std::vector<void*>& shaders;
		std::vector<void*>& vertexBuffers;
		void* indexBuffer;
	};

	union additional_info
	{
		draw_info draw;
	};

	struct local_context
	{
		active_logger& logger;
		void (*printVerbose)();
		additional_info info;
	};
}
