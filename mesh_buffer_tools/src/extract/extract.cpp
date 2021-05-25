#include "input_assembly.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <assert.h>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

#include <nlohmann/json.hpp>

using boost::interprocess::file_mapping;
using boost::interprocess::mapped_region;

typedef std::vector<mbt::input_attribute::attribute_value> vertex;

int main(int argc, char *argv[])
{
	assert(argc > 6);
	std::string descriptor_path	= argv[1];
	std::string output_path		= argv[2];

	int index_count				= std::atoi(argv[3]);
	int first_index				= std::atoi(argv[4]);
	int vertex_offset			= std::atoi(argv[5]);

	std::string index_path		= argv[6];

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
	assert(argc == 7 + bindingCount*2);

	std::vector<std::string> 	buffer_paths	(bindingCount);
	std::vector<int> 			buffer_strides	(bindingCount);
	std::vector<file_mapping>	buffer_mappings	(bindingCount);
	std::vector<mapped_region>	buffer_regions	(bindingCount);
	std::vector<void*>			buffer_pointers	(bindingCount);

	for(int i=0; i<bindingCount; i++)
	{
		buffer_paths[i] = argv[7 + 2*i + 0];
		buffer_strides[i] = std::atoi(argv[7 + 2*i + 1]);

		buffer_mappings[i] = file_mapping(buffer_paths[i].c_str(), boost::interprocess::read_only);
		buffer_regions[i] = mapped_region(buffer_mappings[i], boost::interprocess::read_only);
		buffer_pointers[i] = buffer_regions[i].get_address();
	}

	int vertexCount = buffer_regions[0].get_size() / buffer_strides[0];
	std::vector<vertex> vertices(vertexCount);

	for(int i=0; i<vertexCount; i++)
	{
		vertices[i].resize(attributes.size());
		for(int j=0; j<attributes.size(); j++)
		{
			mbt::input_attribute attr = attributes[j];
			int bufferIndex = std::distance(attributeBindings.begin(),
				std::find(attributeBindings.begin(), attributeBindings.end(), attr.binding));

			void* p = ((uint8_t*)buffer_pointers[bufferIndex]) + buffer_strides[bufferIndex] * i;

			vertices[i][j] = attr.read(p);
		}
	}

	file_mapping m_index(index_path.c_str(), boost::interprocess::read_only);
	mapped_region index_region(m_index, boost::interprocess::read_only);

	std::ofstream out(output_path);
	std::map<int, int> vertexPositions;
	unsigned short* index_buffer = (unsigned short*)index_region.get_address();

	std::optional<int> map_position;
	std::optional<int> map_normal;
	std::optional<int> map_texCoord;
	if(mappings.contains("position"))	map_position	= mappings["position"];
	if(mappings.contains("normal"))		map_normal		= mappings["normal"];
	if(mappings.contains("texCoord"))	map_texCoord	= mappings["texCoord"];

	int face[3];
	int counter = 0;
	for(int i=0; i<index_count; i++)
	{
		int index = index_buffer[i+first_index] + vertex_offset;

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
