#include "layer.hpp"
#include "rules/execution_env.hpp"
#include "rules/ipc.hpp"
#include "rules/reader.hpp"

#include <iostream>

int main()
{
	std::ofstream out("/dev/stdout");
	::logger = new CheekyLayer::logger(out);

	std::string socketRule = R"EOF(
init{} -> seq(
	server_socket(socket, TCP, localhost, 1337, Lines)
)
custom{custom(connect)} -> seq(
	write(socket, string("Hello World")),
	write(socket, string("Ready to receive your matrices"))
)
receive{} -> seq(
	logx(convert(raw, string, received())),
	local=(
		raw,
		matrix,
		reduce(
			map(
				split(
					convert(raw, string, received()),
					" "
				),
				raw,
				pack(
					Float,
					convert(string, number, current_element())
				)
			),
			raw,
			convert(string, raw, string("")),
			concat(current_reduction(), current_element())
		)
	),
	write(socket, local(matrix))
))EOF";

	{
		std::istringstream rulesIn(socketRule);
		CheekyLayer::rules::numbered_streambuf numberer{rulesIn};
		while(rulesIn.good())
		{
			try
			{
				std::unique_ptr<CheekyLayer::rules::rule> rule = std::make_unique<CheekyLayer::rules::rule>();
				rulesIn >> *rule;

				::rules.push_back(std::move(rule));
			}
			catch(const std::exception& ex)
			{
				*logger << CheekyLayer::logger::begin << "Error at " << numberer.line() << ":" << numberer.col() << ":\n\t" << ex.what() << CheekyLayer::logger::end;
				break;
			}
		}
	}
	{
		CheekyLayer::active_logger log = *logger << CheekyLayer::logger::begin;
		CheekyLayer::rules::local_context ctx = { .logger = log };
		CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::Init, VK_NULL_HANDLE, ctx);
		log << CheekyLayer::logger::end;
	}
	{
		*logger << CheekyLayer::logger::begin << "Loaded " << rules.size() << " rules:" << CheekyLayer::logger::end;
		for(auto& r : rules)
		{
			CheekyLayer::active_logger log = *logger << CheekyLayer::logger::begin;
			log << '\t';
			r->print(log.raw());
			log << CheekyLayer::logger::end;
		}
	}
	for(;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}