#include "buffer.hpp"

namespace mbt
{
	buffer::buffer(std::string path_, int stride_, boost::interprocess::mode_t mode)
	{
		path = path_;
		stride = stride_;

		mapping = file_mapping(path.c_str(), mode);
		region = mapped_region(mapping, mode);

		pointer = region.get_address();
	}

	int buffer::count()
	{
		return region.get_size() / stride;
	}

	void* buffer::at(int i)
	{
		return (void*) ( ((uint8_t*)pointer)+ i*stride );
	}

	size_t buffer::size()
	{
		return region.get_size();
	}

	void buffer::flush()
	{
		region.flush();
	}
}
