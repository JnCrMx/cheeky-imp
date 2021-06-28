#pragma once

#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <mutex>
#include <atomic>

namespace CheekyLayer
{
	struct begin_t {};
	struct end_t {};

	class active_logger;

	class logger
	{
		public:
			static const begin_t begin;
			static const end_t end;

			logger(std::ofstream& out_) : out(std::move(out_)) {}
			~logger();

			active_logger operator<<(struct begin_t);
		private:
			std::ofstream out;
			std::mutex lock;
			friend class active_logger;
	};

	class active_logger
	{
		public:
			active_logger(logger* l) : myLogger(l) {}

			void flush();

			template<typename T>
			active_logger& operator<<(T t)
			{
				myLogger->out << t;
				return *this;
			}
			logger& operator<<(struct end_t);
		private:
			logger* myLogger;
	};
}