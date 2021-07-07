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