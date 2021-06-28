#pragma once

#include <functional>
#include <string>
#include <map>

namespace CheekyLayer
{
	class config
	{
		public:
			const static std::function<bool(std::string)> to_bool;

			config() {}
			config(std::map<std::string, std::string> v) : values(v) {}

			bool has(std::string key);
			
			template<typename T>
			T map(std::string key, std::function<T(std::string)> mapper)
			{
				return mapper((*this)[key]);
			}

			std::string operator[](std::string key);
			config operator+(config other);
		private:
			std::map<std::string, std::string> values;

			friend std::istream& operator>>(std::istream&, config&);
	};
	std::istream& operator>>(std::istream& is, config& config);

	extern const config DEFAULT_CONFIG;
}
