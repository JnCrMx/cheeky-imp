#include <algorithm>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

void dfs_util(int v, std::map<int, std::vector<int>>& adj, std::map<int, bool>& visited, std::vector<int>& out)
{
	visited[v] = true;
	out.push_back(v);

	for(int u : adj[v])
		if(!visited[u])
			dfs_util(u, adj, visited, out);
}

int main(int argc, char* argv[])
{
	std::string mesh_path = argv[1];
	std::string bone_path = argv[2];
	std::string result_path = argv[3];

	std::ifstream obj(mesh_path);
	if(!obj)
		throw std::runtime_error("file not found: "+mesh_path);
	
	std::string line;
	std::vector<glm::vec3> vertices;
	std::vector<std::array<int, 3>> faces;
	while(std::getline(obj, line))
	{
		std::istringstream is(line);
		std::string type;
		is >> type;
		if(type=="v")
		{
			float x, y, z;
			is >> x >> y >> z;
			vertices.push_back({x, y, z});
		}
		if(type=="f")
		{
			std::array<std::string, 3> args;
			is >> args[0] >> args[1] >> args[2];

			std::array<int, 3> array;
			for(int i=0; i<3; i++)
			{
				std::stringstream s(args[i]);
				int vertex, uv, normal;

				s >> vertex;
				s.ignore(1);
				s >> uv;
				s.ignore(1);
				s >> normal;

				array[i] = vertex-1;
			}
			faces.push_back(array);
		}
	}

	std::ifstream bones(bone_path);
	if(!bones)
		throw std::runtime_error("file not foind: "+bone_path);
	nlohmann::json json;
	bones >> json;
	std::map<std::string, std::map<std::string, float>> groupVertexWeight = json;

	nlohmann::json result;

	for(auto it = groupVertexWeight.begin(); it != groupVertexWeight.end(); ++it)
	{
		std::vector<std::array<int, 3>> groupFaces;
		std::copy_if(faces.begin(), faces.end(), std::back_inserter(groupFaces), [&it](std::array<int, 3> face){
			for(int i=0; i<3; i++)
			{
				std::string s = std::to_string(face[i]);
				if(!it->second.contains(s))
					return false;
				if(it->second[s] < 0.25f)
					return false;
			}
			return true;
		});
		for(auto& face : groupFaces)
		{
			for(int i=0; i<3; i++)
			{
				glm::vec3 v = vertices[face[i]];
				int first = std::distance(vertices.begin(), std::find(vertices.begin(), vertices.end(), v));
				face[i] = first;
			}
		}

		std::vector<int> edgeLoopVertices;
		
		std::vector<std::tuple<int, int>> edges;
		for(auto face : groupFaces)
		{
			edges.push_back({std::min(face[0], face[1]), std::max(face[0], face[1])});
			edges.push_back({std::min(face[1], face[2]), std::max(face[1], face[2])});
			edges.push_back({std::min(face[0], face[2]), std::max(face[0], face[2])});
		}
		for(auto a : edges)
		{
			int count = std::count(edges.begin(), edges.end(), a);
			if(count == 1)
			{
				edgeLoopVertices.push_back(std::get<0>(a));
				edgeLoopVertices.push_back(std::get<1>(a));
			}
		}
		std::sort(edgeLoopVertices.begin(), edgeLoopVertices.end());
		edgeLoopVertices.erase(std::unique(edgeLoopVertices.begin(), edgeLoopVertices.end()), edgeLoopVertices.end());

		std::cout << "Group " << it->first << ": " << 
			it->second.size() << " vertices and " << 
			groupFaces.size() << " faces and " <<
			edges.size() << " edges and " <<
			edgeLoopVertices.size() << " edge loop vertices." << std::endl;

		if(it->first=="47")
		{
			std::ofstream o("test.obj");
			for(int v : edgeLoopVertices)
			{
				glm::vec3 vec = vertices[v];
				o << "v " << vec.x << " " << vec.y << " " << vec.z << std::endl;
			}
		}

		std::map<int, std::vector<int>> adj;
		for(int i : edgeLoopVertices)
		{
			for(int j : edgeLoopVertices)
			{
				for(auto f : groupFaces)
				{
					if(std::find(f.begin(), f.end(), i) != f.end() &&
						std::find(f.begin(), f.end(), j) != f.end())
					{
						adj[i].push_back(j);
					}
				}
			}
		}
		std::map<int, bool> visited;
		for(int v : edgeLoopVertices)
			visited[v] = false;

		std::vector<std::vector<int>> edgeLoops;
		for(int v : edgeLoopVertices)
		{
			if(!visited[v])
			{
				std::vector<int> out;
				dfs_util(v, adj, visited, out);
				edgeLoops.push_back(out);
			}
		}
		std::cout << "Found " << edgeLoops.size() << " edge loops." << std::endl;
		if(edgeLoops.size() == 2)
		{
			glm::vec3 head = {0, 0, 0};
			for(int i : edgeLoops[0])
			{
				head += vertices[i];
			}
			head /= edgeLoops[0].size();

			glm::vec3 tail = {0, 0, 0};
			for(int i : edgeLoops[1])
			{
				tail += vertices[i];
			}
			tail /= edgeLoops[1].size();

			result[it->first]["head"] = {head.x, head.y, head.z};
			result[it->first]["tail"] = {tail.x, tail.y, tail.z};
		}
		if(edgeLoops.size() == 1)
		{
			glm::vec3 head = {0, 0, 0};
			for(int i : edgeLoops[0])
			{
				head += vertices[i];
			}
			head /= edgeLoops[0].size();

			glm::vec3 tail = {};
			float maxDist = std::numeric_limits<float>::min();
			for(auto jt = it->second.begin(); jt != it->second.end(); ++jt)
			{
				int i = std::stoi(jt->first);
				glm::vec3 v = vertices[i];
				float dist = glm::distance(head, v);
				if(dist > maxDist)
				{
					maxDist = dist;
					tail = v;
				}
			}

			result[it->first]["head"] = {head.x, head.y, head.z};
			result[it->first]["tail"] = {tail.x, tail.y, tail.z};
		}
	}

	{
		std::ofstream out(result_path);
		out << result;
	}

	return 0;
}
