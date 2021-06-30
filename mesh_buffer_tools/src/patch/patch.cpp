#include "input_assembly.hpp"
#include "buffer.hpp"

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <memory>
#include <iostream>
#include <optional>
#include <filesystem>

#include <nlohmann/json.hpp>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

typedef std::vector<mbt::input_attribute::attribute_value> vertex;

void create_file(std::string path, size_t size)
{
	if(std::filesystem::exists(path))
		return;

	std::filebuf fbuf;
	fbuf.open(path, std::ios_base::in | std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
	fbuf.pubseekoff(size-1, std::ios_base::beg);
	fbuf.sputc(0);
}

struct bone_weight {
	int group = 0;
	float weight = 0.0f;
};

#ifdef APPROX_UNIQUE
bool feq(float f1, float f2)
{
	return std::abs(f1-f2) < 0.01f;
}

bool veq2(glm::vec2 v1, glm::vec2 v2)
{
	return feq(v1.x, v2.x) && feq(v1.y, v2.y);
}
bool veq3(glm::vec3 v1, glm::vec3 v2)
{
	return feq(v1.x, v2.x) && feq(v1.y, v2.y) && feq(v1.z, v2.z);
}
bool veq4(glm::vec4 v1, glm::vec4 v2)
{
	return feq(v1.x, v2.x) && feq(v1.y, v2.y) && feq(v1.z, v2.z) && feq(v1.w, v2.w);
}
#endif

struct obj_vertex {
	glm::vec3 position;
	glm::vec2 texCoord;
	glm::vec3 normal;

	glm::uvec4 boneGroups;
	glm::vec4 boneWeights;

	bool operator==(obj_vertex v2)
	{
#ifdef APPROX_UNIQUE
		return
			veq3(position, v2.position) &&
			veq2(texCoord, v2.texCoord) &&
			veq3(normal, v2.normal) &&
			boneGroups == v2.boneGroups &&
			veq4(boneWeights, v2.boneWeights);
#else
		return
			position == v2.position &&
			texCoord == v2.texCoord &&
			normal == v2.normal &&
			boneGroups == v2.boneGroups &&
			boneWeights == v2.boneWeights;
#endif
	}
};

obj_vertex make_vertex(std::tuple<int, int, int>& index,
	std::vector<glm::vec3>& positions, std::vector<glm::vec2>& texCoords, std::vector<glm::vec3>& normals,
	std::map<int, std::vector<bone_weight>>& vertexWeights)
{
	glm::uvec4 boneGroups;
	glm::vec4 boneWeights;
	if(vertexWeights.contains(std::get<0>(index)))
	{
		std::vector<bone_weight> weights = vertexWeights[std::get<0>(index)];
		for(int i=0; i<4; i++)
		{
			boneGroups[i] = weights[i].group;
			boneWeights[i] = weights[i].weight;
		}
	}

	obj_vertex v = {
		positions[std::get<0>(index)],
		texCoords[std::get<1>(index)],
		normals[std::get<2>(index)],

		boneGroups, boneWeights
	};
	return v;
}

int main(int argc, char *argv[])
{
	assert(argc > 6);
	std::string descriptor_path	= argv[1];
	std::string input_path		= argv[2];

	int index_count				= std::atoi(argv[3]);
	int first_index				= std::atoi(argv[4]);
	int vertex_offset			= std::atoi(argv[5]);

	std::string oindex_path		= argv[6];
	std::string pindex_path		= argv[7];

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
	assert(argc == 8 + bindingCount*3);

	std::vector<std::unique_ptr<mbt::buffer>> originalBuffers(bindingCount);
	std::vector<std::unique_ptr<mbt::buffer>> patchBuffers(bindingCount);
	for(int i=0; i<bindingCount; i++)
	{
		originalBuffers[i] = std::make_unique<mbt::buffer>(argv[8 + 3*i + 0], std::atoi(argv[8 + 3*i + 2]), boost::interprocess::mode_t::read_only);

		std::string patchname(argv[8 + 3*i + 1]);
		create_file(patchname, originalBuffers[i]->size());
		patchBuffers[i] = std::make_unique<mbt::buffer>(patchname, std::atoi(argv[8 + 3*i + 2]), boost::interprocess::mode_t::read_write);
	}

	mbt::buffer originalIndexBuffer(oindex_path, 2, boost::interprocess::mode_t::read_only);
	unsigned short* originalIndices = originalIndexBuffer;

	create_file(pindex_path, originalIndexBuffer.size());
	mbt::buffer patchIndexBuffer(pindex_path, 2, boost::interprocess::mode_t::read_write);
	unsigned short* patchIndices = patchIndexBuffer;

	std::vector<int> vertexPositions;
	for(int i=0; i<index_count; i++)
	{
		int vertex = originalIndices[i+first_index];
		if(std::find(vertexPositions.begin(), vertexPositions.end(), vertex) == vertexPositions.end())
			vertexPositions.push_back(vertex);
	}
	std::cout << index_count << " index and " << vertexPositions.size() << " vertex positions usable" << std::endl;

	std::vector<unsigned short> rawIndices;
	std::vector<vertex> vertices;
	std::vector<bool> attributesSet(attributes.size());

	std::optional<int> map_position;
	std::optional<int> map_normal;
	std::optional<int> map_tangent;
	std::optional<int> map_texCoord;
	std::optional<int> map_boneGroups;
	std::optional<int> map_boneWeights;
	if(mappings.contains("position"))		map_position	= mappings["position"];
	if(mappings.contains("normal"))			map_normal		= mappings["normal"];
	if(mappings.contains("tangent"))		map_tangent		= mappings["tangent"];
	if(mappings.contains("texCoord"))		map_texCoord	= mappings["texCoord"];
	if(mappings.contains("bone_groups"))	map_boneGroups	= mappings["bone_groups"];
	if(mappings.contains("bone_weights"))	map_boneWeights	= mappings["bone_weights"];

	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> texCoords;
	std::vector<glm::vec3> normals;
	std::vector<std::tuple<int, int, int>> indices;

	std::ifstream obj(input_path);
	if(!obj)
		throw std::runtime_error("file not found: "+input_path);

	while(std::getline(obj, line))
	{
		std::istringstream is(line);
		std::string type;
		is >> type;
		if(type=="v")
		{
			float x, y, z;
			is >> x >> y >> z;
			positions.push_back({x, y, z});
		}
		if(type=="vt")
		{
			float u, v;
			is >> u >> v;
			texCoords.push_back({u, -v});
		}
		if(type=="vn")
		{
			float x, y, z;
			is >> x >> y >> z;
			normals.push_back({x, y, z});
		}
		if(type=="f")
		{
			std::array<std::string, 3> args;
			is >> args[0] >> args[1] >> args[2];
			for(int i=0; i<3; i++)
			{
				std::stringstream s(args[i]);
				int vertex, uv, normal;

				s >> vertex;
				s.ignore(1);
				s >> uv;
				s.ignore(1);
				s >> normal;

				indices.push_back({vertex-1, uv-1, normal-1});
			}
		}
	}

	std::map<int, std::vector<bone_weight>> vertexWeights;

	std::ifstream bones(input_path+".json");
	if(bones.good())
	{
		nlohmann::json json;
		bones >> json;

		std::map<std::string, std::map<std::string, float>> groupVertexWeight = json;

		for(auto it = groupVertexWeight.begin(); it != groupVertexWeight.end(); ++it)
		{
			for(auto jt = it->second.begin(); jt != it->second.end(); ++jt)
			{
				vertexWeights[std::stoi(jt->first)].push_back({std::stoi(it->first), jt->second});
			}
		}

		for(std::map<int, std::vector<bone_weight>>::iterator it = vertexWeights.begin(); it != vertexWeights.end(); ++it)
		{
			it->second.resize(std::max(it->second.size(), 4ul));
			std::sort(it->second.begin(), it->second.end(), [](bone_weight w1, bone_weight w2) {
				return w1.weight > w2.weight;
			});
			it->second.erase(it->second.begin()+4, it->second.end());

			float sum = 0.0f;
			for(bone_weight w : it->second)
				sum += w.weight;

			if(std::abs(sum-1.0f) > 0.001f)
			{
				std::cout << "Weight sum is " << sum << ": " << it->second[0].weight << " " << it->second[1].weight
					<< " " << it->second[2].weight << " " << it->second[3].weight << std::endl;
			}

			for(bone_weight& w : it->second)
				w.weight /= sum;
		}
	}

	std::vector<obj_vertex> uniqueVertices;
	for(int i=0; i<indices.size(); i++)
	{
		obj_vertex v = make_vertex(indices[i], positions, texCoords, normals, vertexWeights);
		if(std::find(uniqueVertices.begin(), uniqueVertices.end(), v) == uniqueVertices.end())
		{
			uniqueVertices.push_back(v);
		}
	}

	std::cout << indices.size() << " indices and " << uniqueVertices.size() << " vertices found" << std::endl;
	if(indices.size() > index_count)
	{
		std::cerr 	<< "We do not have enough space to fit " << indices.size()
					<< " indices, we have only " << index_count << " positions!" << std::endl;
	}
	if(uniqueVertices.size() > vertexPositions.size())
	{
		std::cerr 	<< "We do not have enough space to fit " << uniqueVertices.size()
					<< " vertices, we have only " << vertexPositions.size() << " positions!" << std::endl;
	}

	vertices.resize(std::min(uniqueVertices.size(), vertexPositions.size()));
	for(int i=0; i<std::min(uniqueVertices.size(), vertexPositions.size()); i++)
	{
		glm::vec3 position 	= uniqueVertices[i].position;
		glm::vec2 texCoord 	= uniqueVertices[i].texCoord;
		glm::vec3 normal 	= uniqueVertices[i].normal;

		vertices[i].resize(attributes.size());
		if(map_position.has_value())
			vertices[i][map_position.value()].vec3 = position;
		if(map_texCoord.has_value())
			vertices[i][map_texCoord.value()].vec2 = texCoord;
		if(map_normal.has_value())
			vertices[i][map_normal.value()].vec4 = glm::vec4(normal, 1.0);
		if(map_tangent.has_value())
			vertices[i][map_tangent.value()].vec4 = glm::vec4(normal.x, 0.0, -normal.y, 1.0); //random tangent lol

		if(map_boneGroups.has_value())
			vertices[i][map_boneGroups.value()].u8vec4 = uniqueVertices[i].boneGroups;
		if(map_boneWeights.has_value())
			vertices[i][map_boneWeights.value()].vec4 = uniqueVertices[i].boneWeights;
	}
	if(map_position.has_value())	attributesSet[map_position.value()] = true;
	if(map_texCoord.has_value())	attributesSet[map_texCoord.value()] = true;
	if(map_normal.has_value())		attributesSet[map_normal.value()] = true;
	if(map_tangent.has_value())		attributesSet[map_tangent.value()] = true;
	if(map_boneGroups.has_value())	attributesSet[map_boneGroups.value()] = true;
	if(map_boneWeights.has_value())	attributesSet[map_boneWeights.value()] = true;

	for(int i=0; i<std::min(indices.size(), static_cast<unsigned long>(index_count)); i++)
	{
		obj_vertex v = make_vertex(indices[i], positions, texCoords, normals, vertexWeights);
		int index = std::distance(uniqueVertices.begin(), std::find(uniqueVertices.begin(), uniqueVertices.end(), v));
		rawIndices.push_back(index);
	}

	for(int i=0; i<vertices.size(); i++)
	{
		for(int j=0; j<attributes.size(); j++)
		{
			if(attributesSet[j])
			{
				mbt::input_attribute attr = attributes[j];
				int bufferIndex = std::distance(attributeBindings.begin(),
					std::find(attributeBindings.begin(), attributeBindings.end(), attr.binding));

				void* p = patchBuffers[bufferIndex]->at(vertexPositions[i] + vertex_offset);

				attr.write(vertices[i][j], p);
			}
		}
	}
	for(int i=0; i<rawIndices.size(); i++)
	{
		patchIndices[first_index + i] = vertexPositions[rawIndices[i]];
	}

	for(std::unique_ptr<mbt::buffer> &buf : patchBuffers)
	{
		buf->flush();
	}
	patchIndexBuffer.flush();

	return 0;
}
