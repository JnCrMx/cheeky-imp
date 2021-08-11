#include "logger.hpp"

namespace CheekyLayer
{
	active_logger logger::operator<<(struct begin_t)
	{
		lock.lock();
		return active_logger(this);
	}

	logger::~logger()
	{
		lock.lock();
		out.flush();
		out.close();
		lock.unlock();
	}

	active_logger& active_logger::operator<<(struct error_t)
	{
		*this << "error: ";
		error = true;
		return *this;
	}

	logger& active_logger::operator<<(struct end_t)
	{
		if(written)
			myLogger->out << '\n';
		myLogger->out.flush();

		myLogger->lock.unlock();

		return *myLogger;
	}

	void active_logger::flush()
	{
		myLogger->out.flush();
	}
}