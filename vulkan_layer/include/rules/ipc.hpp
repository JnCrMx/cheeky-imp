#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
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
			virtual void close() = 0;
			virtual size_t write(std::vector<uint8_t>&) = 0;
			std::string m_name;
	};

	class local_file : public file_descriptor
	{
		public:
			local_file(std::string filename);
			virtual void close();
			virtual size_t write(std::vector<uint8_t>&);
		protected:
			std::unique_ptr<std::ofstream> m_stream;
	};

	class socket : public file_descriptor
	{
		public:
			enum socket_type
			{
				TCP,
				UDP
			};

			enum protocol_type
			{
				Raw,
				LengthPrefixed,
				Lines
			};

			socket(socket_type type, std::string hostname, int port, protocol_type protocol);
			virtual void close();
			virtual size_t write(std::vector<uint8_t>&);

			static socket_type socket_type_from_string(std::string s);
			static std::string socket_type_to_string(socket_type e);
			static protocol_type protocol_type_from_string(std::string s);
			static std::string protocol_type_to_string(protocol_type e);
		protected:
			int m_fd;
			socket_type m_type;
			protocol_type m_protocol;
			std::thread m_receiveThread;
			std::promise<void> m_exitPromise;

			size_t writeRaw(void* p, size_t size);
			void receiveThread(std::future<void> exit);
	};
}
