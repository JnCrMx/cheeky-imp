#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <netinet/in.h>
#include <ostream>
#include <thread>
#include <vector>
#include <string>
#include <memory>
#include <future>

namespace CheekyLayer::rules::ipc
{
	class file_descriptor
	{
		public:
			virtual ~file_descriptor() = default;

			virtual void close() = 0;
			virtual size_t write(std::vector<uint8_t>&, int arg) = 0;
			std::string m_name;
	};

	class local_file : public file_descriptor
	{
		public:
			local_file(std::string filename);
			virtual void close();
			virtual size_t write(std::vector<uint8_t>&, int arg = 0);
		protected:
			std::unique_ptr<std::ofstream> m_stream;
	};

	enum class socket_type
	{
		TCP,
		UDP
	};

	enum class protocol_type
	{
		Raw,
		LengthPrefixed,
		Lines
	};

	void listen_helper(std::stop_token& stop, protocol_type protocol, int fd, std::function<void(uint8_t*, size_t)> handler);

	class socket : public file_descriptor
	{
		public:
			socket(socket_type type, std::string hostname, int port, protocol_type protocol);
			virtual void close();
			virtual size_t write(std::vector<uint8_t>&, int arg = 0);

			static socket_type socket_type_from_string(std::string s);
			static std::string socket_type_to_string(socket_type e);
			static protocol_type protocol_type_from_string(std::string s);
			static std::string protocol_type_to_string(protocol_type e);
		protected:
			int m_fd;
			socket_type m_type;
			protocol_type m_protocol;
			std::jthread m_receiveThread;

			size_t writeRaw(void* p, size_t size);
			void receiveThread(std::stop_token stop);
	};

	class server_socket : public file_descriptor
	{
		public:
			server_socket(socket_type type, std::string hostname, int port, protocol_type protocol);
			virtual void close();
			virtual size_t write(std::vector<uint8_t>&, int client);
		protected:
			int m_fd;
			socket_type m_type;
			protocol_type m_protocol;
			std::jthread m_listenThread;
			std::vector<std::jthread> m_clientThreads{};

			size_t writeRaw(int client, void* p, size_t size);
			void receiveThread(std::stop_token stop, int fd, sockaddr_in addr);
	};
}
