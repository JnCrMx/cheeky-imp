#include "buffer.hpp"
#include "input_assembly.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <filesystem>
#include <assert.h>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

typedef std::vector<mbt::input_attribute::attribute_value> vertex;

int main(int argc, char *argv[])
{
	assert(argc > 7);
	std::string object_name     = argv[1];
	std::string descriptor_path	= argv[2];
	std::string output_path		= argv[3];

	int index_count				= std::atoi(argv[4]);
	std::string index_offsets   = argv[5];
	std::map<unsigned long, std::string> parts = {{0, object_name}};
	if(index_offsets.find('(') != std::string::npos)
	{
		int start = index_offsets.find('(');
		int end = index_offsets.find(')');

		std::string part_offsets = index_offsets.substr(start+1, end-start-1);
		std::istringstream iss(part_offsets);

		std::string part;
		while(std::getline(iss, part, ','))
		{
			int sep = part.find('=');
			std::string name = part.substr(0, sep);
			unsigned long offset = std::stoul(part.substr(sep+1));

			parts[offset] = name;
		}

		index_offsets = index_offsets.substr(0, start);
	}
	int first_index		        = std::stoi(index_offsets);
	int vertex_offset			= std::atoi(argv[6]);

	std::string index_path		= argv[7];

	std::vector<mbt::input_attribute> attributes;
	std::vector<int> attributeBindings;

	std::ifstream descriptor(descriptor_path);
	std::string line;
	while(std::getline(descriptor, line))
	{
		std::istringstream is(line);

		mbt::input_attribute a;
		is >> a;
		attributes.push_back(a);

		attributeBindings.push_back(a.binding);
	}

	std::map<std::string, int> mappings;
	for(int i=0; i<attributes.size(); i++)
	{
		auto name = attributes[i].name;
		if(name == "undefined" || name == "unused" || name == "ignored")
			continue;
		mappings[name] = i;
	}

	std::sort(attributeBindings.begin(), attributeBindings.end());
	auto u = std::unique(attributeBindings.begin(), attributeBindings.end());
	attributeBindings.erase(u, attributeBindings.end());

	int bindingCount = attributeBindings.size();
	assert(argc == 8 + bindingCount*2);

	std::vector<std::unique_ptr<mbt::buffer>> buffers(bindingCount);
	for(int i=0; i<bindingCount; i++)
	{
		auto f = argv[8 + 2*i + 0];
		if(!std::filesystem::exists(f))
		{
			std::cerr << "File not found: " << f << '\n';
			return 2;
		}
		buffers[i] = std::make_unique<mbt::buffer>(f, std::atoi(argv[8 + 2*i + 1]), boost::interprocess::mode_t::read_only);
	}

	int vertexCount = buffers[0]->count();
	std::vector<vertex> vertices(vertexCount);

	for(int i=0; i<vertexCount; i++)
	{
		vertices[i].resize(attributes.size());
		for(int j=0; j<attributes.size(); j++)
		{
			mbt::input_attribute attr = attributes[j];
			int bufferIndex = std::distance(attributeBindings.begin(),
				std::find(attributeBindings.begin(), attributeBindings.end(), attr.binding));

			void* p = buffers[bufferIndex]->at(i);

			vertices[i][j] = attr.read(p);
		}
	}

	mbt::buffer index_buffer(index_path, 2, boost::interprocess::mode_t::read_only);
	unsigned short* indices = index_buffer;

	std::ofstream out(output_path);
	std::map<int, int> vertexPositions;

	std::optional<int> map_position;
	std::optional<int> map_normal;
	std::optional<int> map_texCoord;
	if(mappings.contains("position"))	map_position	= mappings["position"];
	if(mappings.contains("normal"))		map_normal		= mappings["normal"];
	if(mappings.contains("texCoord"))	map_texCoord	= mappings["texCoord"];

	int face[3];
	int counter = 0;
	out << "o " << object_name << std::endl;
	for(int i=0; i<index_count; i++)
	{
		int index = indices[i+first_index] + vertex_offset;

		if(!vertexPositions.contains(index))
		{
			vertex vert = vertices[index];

			if(map_position.has_value())
			{
				glm::vec3 position = vert[map_position.value()].vec3;
				out << "v " << position.x << " " << position.y << " " << position.z << std::endl;
			}
			if(map_normal.has_value())
			{
				glm::vec3 normal = vert[map_normal.value()].vec3;
				out << "vn " << normal.x << " " << normal.y << " " << normal.z << std::endl;
			}
			if(map_texCoord.has_value())
			{
				glm::vec2 texCoord = vert[map_texCoord.value()].vec2;
				out << "vt " << texCoord.x << " " << -texCoord.y << std::endl;
			}

			vertexPositions[index] = counter++;
		}
		face[i%3] = vertexPositions[index]+1;
		if(parts.contains(i))
		{
			out << "g " << parts[i] << std::endl;
			out << "usemtl " << parts[i] << std::endl;
		}
		if((i+1)%3 == 0)
		{
			out << "f "
				<< face[0] << "/" << face[0] << "/" << face[0] << " "
				<< face[1] << "/" << face[1] << "/" << face[1] << " "
				<< face[2] << "/" << face[2] << "/" << face[2] << std::endl;
		}
	}

	if(mappings.contains("bone_groups") && mappings.contains("bone_weights"))
	{
		int groupIndex = mappings["bone_groups"];
		int weightIndex = mappings["bone_weights"];

		std::map<std::string, std::map<std::string, float>> adjusted_groups;
		for(int i=0; i<vertexCount; i++)
		{
			if(!vertexPositions.contains(i))
				continue;
			vertex v = vertices[i];

			auto groups = v[groupIndex].u8vec4;
			auto weights = v[weightIndex].vec4;

			for(int j=0; j<4; j++)
			{
				if(weights[j] > 0.0)
				{
					std::string group = std::to_string(groups[j]);
					std::string index = std::to_string(vertexPositions[i]);
					adjusted_groups[group][index] = weights[j];
				}
			}
		}
		nlohmann::json j = adjusted_groups;
		std::ofstream outJson(output_path+".json");
		outJson << j;
	}

	return 0;
}
