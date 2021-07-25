#pragma once

#include <fstream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <mutex>
#include <atomic>

namespace CheekyLayer
{
	struct begin_t {};
	struct end_t {};
	struct error_t {};

	class active_logger;

	class logger
	{
		public:
			static const begin_t begin;
			static const end_t end;
			static const error_t error;

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
			std::ostream& raw()
			{
				written = true;
				return myLogger->out;
			}

			template<typename T>
			active_logger& operator<<(T t)
			{
				written = true;
				myLogger->out << t;
				return *this;
			}

			active_logger& operator<<(struct error_t);
			logger& operator<<(struct end_t);
		private:
			logger* myLogger;
			bool written = false;
	};
}