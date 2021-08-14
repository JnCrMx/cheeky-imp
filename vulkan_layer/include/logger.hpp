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
	class active_logger;

	class logger
	{
		struct begin_t {};
		struct end_t {};
		struct error_t {};

		public:
			/** Indicator used to begin logging.
			 * To be used with @ref logger::operator<<(struct begin_t begin).
			 */
			static const begin_t begin;
			/** Indicator used to end logging and flush the logger.
			 * To be used with @ref active_logger::operator<<(struct logger::end_t end).
			 */
			static const end_t end;
			/** Indicator to signal that an error is going to be logged.
			 * To be used with @ref active_logger::operator<<(struct logger::end_t end).
			 */
			static const error_t error;

			/** Construct a new logger writing to an std::ostream.
			 * \param out_ Stream to log to. Will be moved from during construction.
			 */
			logger(std::ofstream& out_) : out(std::move(out_)) {}
			~logger();

			/** Begins logging.
			 * \param begin logger::begin
			 * \return An active_logger that can be written to.
			 */
			active_logger operator<<(struct begin_t begin);
		private:
			std::ofstream out;
			std::mutex lock;
			friend class active_logger;
	};

	class active_logger
	{
		public:
			/** Flushes the underlying std::ostream.
			 */
			void flush();

			/** Returns a reference to the raw, underlying std::ostream of this logger.
			 */
			std::ostream& raw()
			{
				written = true;
				return myLogger->out;
			}

			/** Logs a value.
			 * \param t Value to log
			 * \return A reference to this logger, for chaining.
			 */
			template<typename T>
			active_logger& operator<<(T t)
			{
				written = true;
				myLogger->out << t;
				if(error) myLogger->out.flush(); // always flush in error mode, because we have even more potential to crash
				return *this;
			}

			/** Starts logging a error message.
			 * This will log a specialized message ("error: ") and causes
			 * the underlying stream to be flushed after each logged value
			 * for the lifetime of this active_logger.
			 * \param error logger::error
			 * \return A reference to this logger, for chaining.
			 */
			active_logger& operator<<(struct logger::error_t error);

			/** Ends the logging.
			 * This causes a newline ('\n') to be written the underlying stream to be flushed
			 * if any items have been logged before.
			 * Also releases the lock protecting against intertwined log messages.
			 * \param end logger::end
			 * \return A reference to the CheekyLayer::logger this active_logger was derived from.
			 */
			logger& operator<<(struct logger::end_t end);
		private:
			active_logger(logger* l) : myLogger(l) {}

			logger* myLogger;
			bool written = false;
			bool error = false;

			friend class logger;
	};
}