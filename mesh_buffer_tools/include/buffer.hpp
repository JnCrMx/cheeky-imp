#pragma once

#include <cstddef>
#include <string>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

using boost::interprocess::file_mapping;
using boost::interprocess::mapped_region;

namespace mbt
{
	class buffer
	{
		std::string path;
		int stride;
		file_mapping mapping;
		mapped_region region;
		void* pointer;

		public:
			buffer(std::string path, int stride, boost::interprocess::mode_t mode);
			int count();
			size_t size();
			void* at(int i);
			void flush();

			template<typename T> operator T*()
			{
				return (T*) pointer;
			}
	};
}
