#include "rules/ipc.hpp"
#include "layer.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "objects.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

#define LOGGER \
	(m_device ? m_device->logger : m_instance->logger)

namespace CheekyLayer::rules::ipc
{
	local_file::local_file(std::string filename)
	{
		m_stream = std::make_unique<std::ofstream>(filename, std::ios::binary);
	}

	void local_file::close()
	{
		m_stream->close();
	}

	size_t local_file::write(std::vector<uint8_t>& data, int arg)
	{
		m_stream->write((char*)data.data(), data.size());
		return data.size();
	}

	socket_type socket::socket_type_from_string(std::string s)
	{
		if(s=="TCP")
			return socket_type::TCP;
		if(s=="UDP")
			return socket_type::UDP;
		throw std::runtime_error("unsupported socket_type: "+s);
	}

	std::string socket::socket_type_to_string(socket_type e)
	{
		switch(e)
		{
			case socket_type::TCP:
				return "TCP";
			case socket_type::UDP:
				return "UDP";
			default:
				return "unknown";
		}
	}

	protocol_type socket::protocol_type_from_string(std::string s)
	{
		if(s=="Raw")
			return protocol_type::Raw;
		if(s=="LengthPrefixed")
			return protocol_type::LengthPrefixed;
		if(s=="Lines")
			return protocol_type::Lines;
		throw std::runtime_error("unsupported protocol_type: "+s);
	}

	std::string socket::protocol_type_to_string(protocol_type e)
	{
		switch(e)
		{
			case protocol_type::Raw:
				return "Raw";
			case protocol_type::LengthPrefixed:
				return "LengthPrefixed";
			case protocol_type::Lines:
				return "Lines";
			default:
				return "unknown";
		}
	}

	socket::socket(socket_type type, std::string hostname, int port, protocol_type protocol) : m_type(type) , m_protocol(protocol)
	{
		m_fd = ::socket(AF_INET, type == socket_type::TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
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

		m_receiveThread = std::jthread(&socket::receiveThread, this);
	}

	void socket::close()
	{
		m_receiveThread.request_stop();
		::close(m_fd);
	}

	size_t socket::writeRaw(void* buf, size_t size)
	{
		return ::write(m_fd, buf, size);
	}

	size_t socket::write(std::vector<uint8_t>& data, int arg)
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
	void listen_helper(std::stop_token& stop, protocol_type protocol, int fd, std::function<void(uint8_t*, size_t)> handler)
	{
		if(protocol == protocol_type::Raw)
		{
			std::vector<uint8_t> buffer(RECEIVE_BUFFER_SIZE);
			while(!stop.stop_requested())
			{
				struct pollfd pfd = { fd, POLLIN , 0 };
				if(poll(&pfd, 1, 100) > 0)
				{
					int n;
					int total = 0;
					try
					{
						while((n = recv(fd, buffer.data() + total, RECEIVE_BUFFER_SIZE, MSG_DONTWAIT)) > 0)
						{
							total += n;
							if(n < RECEIVE_BUFFER_SIZE)
								break;
							buffer.resize(total + RECEIVE_BUFFER_SIZE);
						}
						if(n == 0) break;
						if(n < 0 && n != EAGAIN && n != EWOULDBLOCK) break;
						buffer.resize(total);
					}
					catch(const std::exception& ex)
					{
						*::logger << logger::begin << logger::error << "receive failed: " << ex.what() << logger::end;
					}

					handler(buffer.data(), buffer.size());

					total = 0;
					buffer.resize(RECEIVE_BUFFER_SIZE);
				}
			}
		}
		else if(protocol == protocol_type::LengthPrefixed)
		{
			while(!stop.stop_requested())
			{
				struct pollfd pfd = { fd, POLLIN , 0 };
				if(poll(&pfd, 1, 100) > 0)
				{
					ssize_t n; // unsigned is ok here, as we do no <> comparison
					size_t size;
					if((n = read(fd, &size, sizeof(size))) == sizeof(size))
					{
						uint8_t* buffer = new uint8_t[size];

						n = read(fd, buffer, size);

						handler(buffer, n);
					}
					else if(n == 0) break;
					else if(n < 0) break;
				}
			}
		}
		else if(protocol == protocol_type::Lines)
		{
			std::string buffer = "";
			while(!stop.stop_requested())
			{
				struct pollfd pfd = { fd, POLLIN , 0 };
				if(poll(&pfd, 1, 100) > 0)
				{
					char buf[RECEIVE_BUFFER_SIZE];
					ssize_t n = recv(fd, buf, RECEIVE_BUFFER_SIZE, 0);
					if(n == 0) break;
					if(n < 0) break;

					auto pos = std::find(buf, buf+RECEIVE_BUFFER_SIZE, '\n');
					if(pos != (buf+RECEIVE_BUFFER_SIZE))
					{
						std::string data = buffer + std::string(buf, std::distance(buf, pos));
						buffer = std::string(pos+1, std::distance(pos+1, buf+n)); // pos+1 -> skip \n

						handler((uint8_t*)data.data(), data.size());
					}
					else
					{
						buffer += std::string(buf, buf + n);
					}
				}
			}
		}
	}

	void socket::receiveThread(std::stop_token stop)
	{
		listen_helper(stop, m_protocol, m_fd, [this](uint8_t* data, size_t size){
			receive_info info = { .socket = this, .buffer = data, .size = size };
			calling_context ctx{
				.info = info,
				.local_variables = {{"socket:name", m_name}}
			};

			if(m_device) {
				m_device->execute_rules(selector_type::Receive, VK_NULL_HANDLE, ctx);
			} else if(m_instance) {
				m_instance->execute_rules(selector_type::Receive, VK_NULL_HANDLE, ctx);
			}
		});
		LOGGER->info("receiving thread for fd \"{}\" exited", m_name);
	}

	server_socket::server_socket(socket_type type, std::string hostname, int port, protocol_type protocol) : m_type(type) , m_protocol(protocol)
	{
		m_fd = ::socket(AF_INET, type == socket_type::TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
		if(m_fd < 0)
		{
			throw std::runtime_error("cannot open socket of type " + socket::socket_type_to_string(type) + ": " + strerror(errno));
		}

		struct hostent* hp = gethostbyname(hostname.c_str());
		if(hp == NULL)
		{
			::close(m_fd);
			throw std::runtime_error("unknown host");
		}

		int one = 1;
		if(::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0)
		{
			::close(m_fd);
			throw std::runtime_error(std::string{"failed to set socket option: "} + strerror(errno));
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
		addr.sin_port = htons(port);

		if(bind(m_fd, (sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
		{
			::close(m_fd);
			throw std::runtime_error("failed to bind to " + hostname + ":" + std::to_string(port) + ": " + strerror(errno));
		}
		if(listen(m_fd, 3) < 0)
		{
			::close(m_fd);
			throw std::runtime_error("failed to listen at " + hostname + ":" + std::to_string(port) + ": " + strerror(errno));
		}

		m_listenThread = std::jthread{[this](std::stop_token stop){
			while(!stop.stop_requested())
			{
				struct sockaddr_in addr;
				size_t addrlen = sizeof(addr);
				int fd = accept(m_fd, (sockaddr*)&addr, (socklen_t*)&addrlen);
				if(fd >= 0)
				{
					m_clientThreads.push_back(std::jthread{&server_socket::receiveThread, this, fd, addr});
				}
			}
		}};
	}

	void server_socket::close()
	{
		m_listenThread.request_stop();
		for(auto& t : m_clientThreads)
		{
			t.request_stop();
		}
		::close(m_fd);
	}

	void server_socket::receiveThread(std::stop_token stop, int fd, sockaddr_in addr)
	{
		std::string ip = inet_ntoa(addr.sin_addr);
		int port = htons(addr.sin_port);
		{
			LOGGER->info("accepted client {} ({}:{}) for server {}", fd, ip, port, m_name);

			receive_info info = { .socket = this, .buffer = nullptr, .size = 0, .extra = fd };
			calling_context ctx{
				.info = info,
				.customTag = "connect",
				.local_variables = {{"socket:name", m_name}}
			};

			if(m_device) {
				m_device->execute_rules(selector_type::Custom, VK_NULL_HANDLE, ctx);
			} else if(m_instance) {
				m_instance->execute_rules(selector_type::Custom, VK_NULL_HANDLE, ctx);
			}
		}

		listen_helper(stop, m_protocol, fd, [this, fd](uint8_t* data, size_t size){
			receive_info info = { .socket = this, .buffer = data, .size = size };
			calling_context ctx{
				.info = info,
				.local_variables = {{"socket:name", m_name}}
			};

			if(m_device) {
				m_device->execute_rules(selector_type::Receive, VK_NULL_HANDLE, ctx);
			} else if(m_instance) {
				m_instance->execute_rules(selector_type::Receive, VK_NULL_HANDLE, ctx);
			}
		});
		LOGGER->info("client {} ({}:{}) for server {} disconnected", fd, ip, port, m_name);
	}

	size_t server_socket::write(std::vector<uint8_t>& data, int fd)
	{
		switch(m_protocol)
		{
			case protocol_type::Raw:
				return writeRaw(fd, data.data(), data.size());
			case protocol_type::LengthPrefixed: {
				size_t size = data.size();
				writeRaw(fd, &size, sizeof(size));
				return writeRaw(fd, data.data(), data.size());
			}
			case protocol_type::Lines: {
				size_t written = writeRaw(fd, data.data(), data.size());
				char newLine = '\n';
				writeRaw(fd, &newLine, sizeof(newLine));
				return written;
			}
		}
		return -1;
	}

	size_t server_socket::writeRaw(int fd, void* buf, size_t size)
	{
		return ::write(fd, buf, size);
	}
}
