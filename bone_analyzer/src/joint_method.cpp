#include <algorithm>
#include <glm/common.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <iostream>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <iomanip>
#include <experimental/iterator>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <nlohmann/json.hpp>

glm::vec3 bounding_box_center(std::vector<glm::vec3>& vertices)
{
	glm::vec3 min{std::numeric_limits<glm::vec3::value_type>::max()}, max{std::numeric_limits<glm::vec3::value_type>::lowest()};
	for(auto v : vertices)
	{
		if(v.x > max.x) max.x = v.x;
		if(v.y > max.y) max.y = v.y;
		if(v.z > max.z) max.z = v.z;

		if(v.x < min.x) min.x = v.x;
		if(v.y < min.y) min.y = v.y;
		if(v.z < min.z) min.z = v.z;
	}

	return (min+max)/2.0f;
}

glm::vec3 average_center(std::vector<glm::vec3>& vertices)
{
	glm::vec3 sum{0.0};
	for(auto v : vertices)
		sum += v;
	return sum / (float)vertices.size();
}

// custom function for computing the center of a vertex group
glm::vec3 center(std::vector<glm::vec3>& vertices)
{
	if(vertices.size() == 0)
		return glm::vec3{0};

	//return bounding_box_center(vertices);
	//return average_center(vertices);
	return (bounding_box_center(vertices)+average_center(vertices))/2.0f;
}

// wrapper for center(std::vector<glm::vec3>& vertices) that transforms indices and all vertices into selected vertices
glm::vec3 center(std::vector<int>& indices, std::vector<glm::vec3>& vertices)
{
	std::vector<glm::vec3> vs(indices.size());
	std::transform(indices.begin(), indices.end(), vs.begin(), [&vertices](int i){
		return vertices[i];
	});
	return center(vs);
}

bool analyzeGroup(
	int										group, 				// "our" group / the bone group we are currently analyzing
	std::map<int, std::map<int, float>>&	map,				// a map (vertex -> (group -> weight)) of all vertices (by index)
	std::vector<glm::vec3>&					vertices,			// positions (vertex -> position) of all vertices (index -> position)
	std::vector<int>&						visited,			// a list of all groups that have already been visited
	nlohmann::json&							out, 				// json output
	int 									parent		= -1,	// our parent group
	glm::vec3								start		= {},	// joint of our parent and us
	glm::vec3								parentStart	= {}, 	// joint of our parent and its parent
	std::vector<int> 						willVisit	= {})	// list of groups that will potentially be visisted by one of our anchestors, unless we visit them first
{
	visited.push_back(group);
	std::cout << "Group " << group << " (parent " << parent << ")" << std::endl;
	out["group"] = group;

	int sharedCount = 0;

	std::map<int, std::vector<int>> otherGroups;
	std::vector<int> allVertices;
	std::vector<int> exclusive;

	for(auto& [vertex, groups] : map) // find other groups and exclusive vertices
	{
		if(groups.contains(group))
		{
			allVertices.push_back(vertex);
			if(groups.size() == 1)
				exclusive.push_back(vertex);
			else
			{
				auto max = std::max_element(groups.begin(), groups.end(), [group](auto a, auto b){
					if(a.first == group) return true;
					if(b.first == group) return false;
					return a.second < b.second;
				}); // only use the group with the largest weight (except for ourself ofc), not sure if this makes things better or worse, but I'm too afraid to experiment with it
				otherGroups[max->first].push_back(vertex);

				sharedCount++;
			}
		}
	}

	// debug output
	std::cout << exclusive.size() << " exclusive vertices" << std::endl;
	std::cout << sharedCount << " shared vertices in " << otherGroups.size() << " groups: ";
	std::transform(otherGroups.begin(), otherGroups.end(), std::experimental::make_ostream_joiner(std::cout, ", "), [](auto a){
		return std::to_string(a.first)+"("+std::to_string(a.second.size())+")";
	});
	std::cout << std::endl;

	// store some information that might be useful
	out["exclusiveVertexCount"] = exclusive.size();
	out["sharedVertexCount"] = sharedCount;
	out["groupCount"] = otherGroups.size();

	if(sharedCount == 0) // this analyzer cannot handle unconnected bones yet
		return false;

	glm::vec3 tail{0.0};
	glm::vec3 mycenter = center(allVertices, vertices);

	if(otherGroups.size() == 1) // we are at the end of a tree
	{
		glm::vec3 avg;
		if(exclusive.empty())
			avg = mycenter; // if (below) is not possible, just use our own center as fallback
		else
			avg = center(exclusive, vertices); // if possible, use only exclusive vertices to achieve a better distance from out parent

		std::cout << glm::to_string(start) << " " << glm::to_string(avg) << std::endl;
		out["head"] = {start.x, start.y, start.z};
		out["tail"] = {avg.x, avg.y, avg.z};
		tail = avg;
	}
	else if(otherGroups.size() == 2) // best case, make the bone go from the parent to the child
	{
		std::vector<glm::vec3> avgs;
		int p = 0;
		for(auto& [g, vs] : otherGroups)
		{
			if(g==parent && start != glm::vec3{})
			{
				p = avgs.size();
				avgs.push_back(start);
				continue;
			}
			avgs.push_back(center(vs, vertices));
		}

		glm::vec3 head = avgs[p];
		tail = avgs[p==1?0:1];
		std::cout << glm::to_string(head) << " " << glm::to_string(tail) << std::endl;

		out["head"] = {head.x, head.y, head.z};
		out["tail"] = {tail.x, tail.y, tail.z};
	}
	else // worst case, for 3 or more groups use the group that is the farest away
	{
		glm::vec3 head = start;
		float maxDistance = 0;

		float avgVertices = std::accumulate(otherGroups.begin(), otherGroups.end(), 0.0, [parent](float a, auto b){
			if(b.first == parent)
				return a;
			return a + b.second.size();
		}) / (otherGroups.size() - 1);
		std::cout << "Average vertex count per group: " << avgVertices << std::endl;

		for(auto& [g, vs] : otherGroups)
		{
			if(g == parent)
				continue;

			if(vs.size() < avgVertices*0.75) // ignore groups under 75% average size (e.g. with only a few vertices)
				continue;
			
			glm::vec3 c = center(vs, vertices);
			if(glm::length(c) > 1000.0f) // avoid groups with broken distance
				continue;

			float dist = glm::distance(c, head);
			if(dist > maxDistance)
			{
				tail = c;
				maxDistance = dist;
			}
		}
		if(tail == glm::vec3{0.0})
		{
			tail = mycenter; // if we cannot find any tail just use our own center
		}

		std::cout << glm::to_string(head) << " " << glm::to_string(tail) << std::endl;

		out["head"] = {head.x, head.y, head.z};
		out["tail"] = {tail.x, tail.y, tail.z};
	}
	std::cout << std::endl;
	
	nlohmann::json& children = out["children"] = nlohmann::json::array();

	std::vector<std::pair<int, std::vector<int>>> others(otherGroups.size());
	std::copy(otherGroups.begin(), otherGroups.end(), others.begin());

	// decend into closer groups first (to avoid skipping B in A-B-C if we also share vertices with C)
	std::sort(others.begin(), others.end(), [mycenter, &vertices](auto& a, auto& b){
		auto c1 = center(a.second, vertices);
		auto c2 = center(b.second, vertices);
		return glm::distance(c1, mycenter) < glm::distance(c2, mycenter); // not sure why we don't use "start" instead of "mycenter", but I'm not gonna mess with this
	});

	std::vector<int> willVisitAfter = willVisit; // we need to make a copy here, or (3) will always be true
	std::transform(others.begin(), others.end(), std::back_inserter(willVisitAfter), [](auto a){
		return a.first;
	});
	for(auto& [g, vs] : others)
	{
		if(std::find(visited.begin(), visited.end(), g) == visited.end()) // don't visit groups again
		{
			glm::vec3 avg = center(vs, vertices);

			// skip decending if the group is a probable siblings
			if(glm::distance(tail, avg) > glm::distance(start, avg) && // (1) detect siblings by checking if they are closer to our head than our tail
					parent != -1 && // (2) the root of the tree cannot have siblings
					std::find(willVisit.begin(), willVisit.end(), g) != willVisit.end()) // (3) a higher level group needs to "plan" to decend into the group or it is no sibling
				continue;

			nlohmann::json& child = children.emplace_back();
			analyzeGroup(g, map, vertices, visited, child, group, avg, start, willVisitAfter);
		}
	}

	return true;
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
	std::vector<int> allFaces;

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
				allFaces.push_back(vertex-1);
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

	std::map<int, std::map<int, float>> vertexGroupWeight;
	for(auto& [name, vertices] : groupVertexWeight)
	{
		for(auto [vertex, weight] : vertices)
		{
			if(weight < 0.1)
				continue;

			int v = std::stoi(vertex);
			if(std::find(allFaces.begin(), allFaces.end(), v) == allFaces.end())
				continue;

			vertexGroupWeight[v][std::stoi(name)] = weight;
		}
	}

	glm::vec3 c = center(vertices);

	int startGroup = -1;
	float minDist = std::numeric_limits<float>::max();

	std::cout << glm::to_string(c) << std::endl;
	for(auto& [name, vs] : groupVertexWeight)
	{
		std::vector<int> v(vs.size());
		std::transform(vs.begin(), vs.end(), v.begin(), [](auto a){
			return std::stoi(a.first);
		});

		glm::vec3 cc = center(v, vertices);
		float d = glm::distance(c, cc);
		if(d < minDist)
		{
			startGroup = std::stoi(name);
			minDist = d;
		}
	}
	std::cout << "Most centered group is " << startGroup << " with distance " << minDist << " to center " << glm::to_string(c) << std::endl;

	std::vector<int> visited;

	if(argc > 4)
		startGroup = std::atoi(argv[4]);
	std::cout << "Starting at group " << startGroup << '\n' << std::endl;

	nlohmann::json out;
	nlohmann::json& array = out["trees"] = nlohmann::json::array();
	nlohmann::json& tree = array.emplace_back();
	analyzeGroup(startGroup, vertexGroupWeight, vertices, visited, tree, -1, c);

	std::cout << "Visited " << visited.size() << " of " << groupVertexWeight.size() << " groups" << std::endl;

	while(visited.size() < groupVertexWeight.size())
	{
		auto start = std::max_element(groupVertexWeight.begin(), groupVertexWeight.end(), [&visited](auto& a, auto& b){
			bool av = std::find(visited.begin(), visited.end(), std::stoi(a.first)) != visited.end();
			bool bv = std::find(visited.begin(), visited.end(), std::stoi(b.first)) != visited.end();

			if(av != bv)
			{
				if(av) return true;
				if(bv) return false;
			}
			return a.second.size() < b.second.size();
		});

		int s = std::stoi(start->first);
		std::vector<int> v(start->second.size());
		std::transform(start->second.begin(), start->second.end(), v.begin(), [](auto a){
			return std::stoi(a.first);
		});
		glm::vec3 gc = center(v, vertices);

		nlohmann::json& tree = array.emplace_back();
		analyzeGroup(s, vertexGroupWeight, vertices, visited, tree, -1, gc);
		std::cout << "Visited " << visited.size() << " of " << groupVertexWeight.size() << " groups" << std::endl;
	}

	{
		std::ofstream result(result_path);
		result << std::setw(4) << out << std::endl;
	}
}
