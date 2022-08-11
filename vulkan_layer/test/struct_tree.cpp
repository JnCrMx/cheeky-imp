#include <assert.h>
#include <iostream>
#include <ostream>
#include <iomanip>
#include <vector>
#include <algorithm>

#include "reflection/vkreflection.hpp"

using CheekyLayer::reflection::struct_reflection_map;
using CheekyLayer::reflection::VkReflectInfo;

void dump_struct(std::ostream& out, std::string name, int level = 0, uint64_t last = 0)
{
	if(!struct_reflection_map.contains(name))
	{
		for(int j=0; j<level; j++)
		{
			if((last & (1<<j)))
				out << " ";
			else
				out << "│";
			out << " ";
		}
		out << "└──(struct not found)" << std::endl;
		return;
	}

	auto& a = struct_reflection_map[name];

	std::vector<VkReflectInfo> vec;
	for(auto it = a.members.begin(); it != a.members.end(); it++)
	{
		vec.push_back(it->second);
	}

	std::sort(vec.begin(), vec.end(), [](VkReflectInfo a, VkReflectInfo b) {
		return a.offset < b.offset;
	});

	for(int i=0; i<vec.size(); i++)
	{
		VkReflectInfo info = vec[i];

		for(int j=0; j<level; j++)
		{
			if((last & (1<<j)))
				out << " ";
			else
				out << "│";
			out << " ";
		}

		bool tlast = i == vec.size()-1;
		if(tlast)
			out << "└";
		else
			out << "├";
		out << "─";

		bool tree = struct_reflection_map.contains(info.type);
		if(tree)
			out << "┬";
		else
			out << "─";
		out << "─";

		out << std::left << std::setfill(' ') << std::setw(50-level*2) << info.name
			<< " @ 0x" << std::right << std::setfill('0') << std::setw(2) << std::hex << info.offset
			<< " : " << info.type;
		if(info.pointer)
			out << "*";
		out << std::endl;
		if(tree)
			dump_struct(out, info.type, level+1, last | (tlast ? (1<<level) : 0));
	}
}

int main(int argc, char* argv[])
{
	if(argc != 2 || std::string(argv[1]) == "--help")
	{
		std::cerr << "Usage: struct_tree STRUCT_NAME" << std::endl;
		return 2;
	}

	std::string entry = argv[1];

	std::cout << entry << std::endl;
	dump_struct(std::cout, entry);
}
