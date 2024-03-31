#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace CheekyLayer
{
	class config
	{
		public:
			const static std::function<bool(std::string)> to_bool;

			config() : config(std::unordered_map<std::string, std::string>{}) {}
			config(const std::unordered_map<std::string, std::string>& v);

			bool has(const std::string& key);

			template<typename T>
			T map(const std::string& key, std::function<T(std::string)> mapper)
			{
				return mapper((*this)[key]);
			}
			const std::string& operator[](const std::string& key) const;

			std::filesystem::path log_file;
			std::string application;
			bool hook_draw_calls;
			std::filesystem::path rule_file;

			bool dump;
			bool dump_png;
			bool dump_png_flipped;
			std::filesystem::path dump_directory;

			bool override;
			bool override_png_flipped;
			std::filesystem::path override_directory;
		private:
			std::unordered_map<std::string, std::string> values;

			friend std::istream& operator>>(std::istream&, config&);
	};
	std::istream& operator>>(std::istream& is, config& config);

	extern const config DEFAULT_CONFIG;
}
