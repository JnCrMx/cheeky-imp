#include "rules/ipc.hpp"
#include "logger.hpp"
#include "layer.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <future>
#include <ios>

#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

namespace CheekyLayer { namespace ipc
{
	local_file::local_file(std::string filename)
	{
		m_stream = std::make_unique<std::ofstream>(filename, std::ios::binary);
	}

	void local_file::close()
	{
		m_stream->close();
	}

	size_t local_file::write(std::vector<uint8_t>& data)
	{
		m_stream->write((char*)data.data(), data.size());
		return data.size();
	}

	socket::socket_type socket::socket_type_from_string(std::string s)
	{
		if(s=="TCP")
			return TCP;
		if(s=="UDP")
			return UDP;
		throw std::runtime_error("unsupported socket_type: "+s);
	}

	std::string socket::socket_type_to_string(socket::socket_type e)
	{
		switch(e)
		{
			case TCP:
				return "TCP";
			case UDP:
				return "UDP";
			default:
				return "unknown";
		}
	}

	socket::protocol_type socket::protocol_type_from_string(std::string s)
	{
		if(s=="Raw")
			return Raw;
		if(s=="LengthPrefixed")
			return LengthPrefixed;
		if(s=="Lines")
			return Lines;
		throw std::runtime_error("unsupported protocol_type: "+s);
	}

	std::string socket::protocol_type_to_string(protocol_type e)
	{
		switch(e)
		{
			case Raw:
				return "Raw";
			case LengthPrefixed:
				return "LengthPrefixed";
			case Lines:
				return "Lines";
			default:
				return "unknown";
		}
	}

	socket::socket(socket_type type, std::string hostname, int port, protocol_type protocol) : m_type(type) , m_protocol(protocol)
	{
		m_fd = ::socket(AF_INET, type == TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
		if(m_fd < 0)
		{
			throw std::runtime_error("cannot open socket of type " + socket_type_to_string(type) + ": " + strerror(errno));
		}

		struct hostent* hp = gethostbyname(hostname.c_str());
		if(hp == NULL)
		{
			::close(m_fd);
			throw std::runtime_error("unknown host");
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
		addr.sin_port = htons(port);

		if(connect(m_fd, (sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
		{
			::close(m_fd);
			throw std::runtime_error("failed to connect to " + hostname + ":" + std::to_string(port) + ": " + strerror(errno));
		}

		std::future<void> future = m_exitPromise.get_future();
		m_receiveThread = std::thread(&socket::receiveThread, this, std::move(future));
	}

	void socket::close()
	{
		m_exitPromise.set_value();
		m_receiveThread.join();
		::close(m_fd);
	}

	size_t socket::writeRaw(void* buf, size_t size)
	{
		return ::write(m_fd, buf, size);
	}

	size_t socket::write(std::vector<uint8_t>& data)
	{
		switch(m_protocol)
		{
			case protocol_type::Raw:
				return writeRaw(data.data(), data.size());
			case protocol_type::LengthPrefixed: {
				size_t size = data.size();
				writeRaw(&size, sizeof(size));
				return writeRaw(data.data(), data.size());
			}
			case protocol_type::Lines: {
				size_t written = writeRaw(data.data(), data.size());
				char newLine = '\n';
				writeRaw(&newLine, sizeof(newLine));
				return written;
			}
		}
		return -1;
	}

	constexpr size_t RECEIVE_BUFFER_SIZE = 1024;
	void socket::receiveThread(std::future<void> exit)
	{
		if(m_protocol == protocol_type::Raw)
		{
			std::vector<uint8_t> buffer(RECEIVE_BUFFER_SIZE);
			while(true)
			{
				struct pollfd pfd = { m_fd, POLLIN , 0 };
				if(poll(&pfd, 1, 100) > 0)
				{
					int n;
					int total = 0;
					try
					{
						while((n = recv(m_fd, buffer.data() + total, RECEIVE_BUFFER_SIZE, MSG_DONTWAIT)) > 0)
						{
							total += n;
							if(n < RECEIVE_BUFFER_SIZE)
								break;
							buffer.resize(total + RECEIVE_BUFFER_SIZE);
						}
						buffer.resize(total);
					}
					catch(const std::exception& ex)
					{
						*::logger << logger::begin << logger::error << "receive failed: " << ex.what() << logger::end;
					}

					active_logger log = *::logger << logger::begin;

					receive_info info = { .socket = this, .buffer = buffer.data(), .size = buffer.size() };
					additional_info info2 = { .receive = info };
					local_context ctx = { .logger = log, .info = &info2 };

					execute_rules(rules, selector_type::Receive, 0, ctx);

					log << logger::end;

					total = 0;
					buffer.resize(RECEIVE_BUFFER_SIZE);
				}
				else
				{
					if(exit.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
						break;
				}
			}
		}
		else if(m_protocol == protocol_type::LengthPrefixed)
		{
			while(true)
			{
				struct pollfd pfd = { m_fd, POLLIN , 0 };
				if(poll(&pfd, 1, 100) > 0)
				{
					size_t n; // unsigned is ok here, as we do no <> comparison
					size_t size;
					if((n = read(m_fd, &size, sizeof(size))) == sizeof(size))
					{
						uint8_t* buffer = new uint8_t[size];

						n = read(m_fd, buffer, size);
				
						active_logger log = *::logger << logger::begin;

						receive_info info = { .socket = this, .buffer = buffer, .size = n };
						additional_info info2 = { .receive = info };
						local_context ctx = { .logger = log, .info = &info2 };

						execute_rules(rules, selector_type::Receive, 0, ctx);

						log << logger::end;
					}
				}
				else
				{
					if(exit.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
						break;
				}
			}
		}
		else if(m_protocol == protocol_type::Lines)
		{
			std::string buffer = "";
			while(true)
			{
				struct pollfd pfd = { m_fd, POLLIN , 0 };
				if(poll(&pfd, 1, 100) > 0)
				{
					char buf[RECEIVE_BUFFER_SIZE];
					size_t n = read(m_fd, buf, RECEIVE_BUFFER_SIZE);
					auto pos = std::find(buf, buf+RECEIVE_BUFFER_SIZE, '\n');
					if(pos != (buf+RECEIVE_BUFFER_SIZE))
					{
						std::string data = buffer + std::string(buf, std::distance(buf, pos));
						buffer = std::string(pos, std::distance(pos, buf+n));

						active_logger log = *::logger << logger::begin;

						receive_info info = { .socket = this, .buffer = (uint8_t*)data.data(), .size = data.size() };
						additional_info info2 = { .receive = info };
						local_context ctx = { .logger = log, .info = &info2 };

						execute_rules(rules, selector_type::Receive, 0, ctx);

						log << logger::end;
					}
					else
					{
						buffer += std::string(buf, buf + n);
					}
				}
				else
				{
					if(exit.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
						break;
				}
			}
		}
		if(::logger)
			*::logger << logger::begin << "receiving thread for fd \"" << m_name << "\" exited" << logger::end;
	}
}}
