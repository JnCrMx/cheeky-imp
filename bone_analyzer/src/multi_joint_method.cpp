#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <ios>
#include <iterator>
#include <numbers>
#include <numeric>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <experimental/iterator>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <nlohmann/json.hpp>

constexpr double operator""_deg(long double d)
{
	return (d*std::numbers::pi)/180.0;
}

std::ostream& operator<<(std::ostream& out, glm::vec3 vec)
{
	return out << glm::to_string(vec);
}

// namespace with configurable constants
namespace constants
{
	// constants for determining bone tail for 3 or more groups
	constexpr float brokenDistance = 100.0f;	// consider a group "broken" is its distance to the bone is greater than this value
	constexpr float groupDropout = 0.75;		// remove groups with a vertex count under "groupDropout * average vertex count"

	// constants for initial reading of bones
	constexpr float minWeight = 0.075;
}

struct Mesh
{
	std::string name;
	std::string meshObj;
	std::string bonesJson;
	std::string outputJson;

	std::vector<glm::vec3> vertices;

	std::map<int, std::map<int, float>> groupVertexWeight;
	std::map<int, std::map<int, float>> vertexGroupWeight;
};

glm::vec3 bounding_box_center(const std::vector<glm::vec3>& vertices)
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

glm::vec3 average_center(const std::vector<glm::vec3>& vertices)
{
	glm::vec3 sum{0.0};
	for(auto v : vertices)
		sum += v;
	return sum / (float)vertices.size();
}

std::pair<glm::vec3, glm::vec3> boundingBox(const std::vector<glm::vec3>& vertices)
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
	return {min, max};
}
std::pair<glm::vec3, glm::vec3> boundingBox(const std::vector<int>& indices, const std::vector<glm::vec3>& vertices)
{
	std::vector<glm::vec3> vs(indices.size());
	std::transform(indices.begin(), indices.end(), vs.begin(), [&vertices](int i){
		return vertices[i];
	});
	return boundingBox(vs);
}

// custom function for computing the center of a vertex group
glm::vec3 center(const std::vector<glm::vec3>& vertices)
{
	if(vertices.size() == 0)
		return glm::vec3{0};

	//return bounding_box_center(vertices);
	//return average_center(vertices);
	return (bounding_box_center(vertices)+average_center(vertices))/2.0f;
}

// wrapper for center(std::vector<glm::vec3>& vertices) that transforms indices and all vertices into selected vertices
glm::vec3 center(const std::vector<int>& indices, const std::vector<glm::vec3>& vertices)
{
	std::vector<glm::vec3> vs(indices.size());
	std::transform(indices.begin(), indices.end(), vs.begin(), [&vertices](int i){
		return vertices[i];
	});
	return center(vs);
}

struct BoneInfo
{
	int parent;
	glm::vec3 head;
	glm::vec3 tail;
	std::vector<int> childs;
};

std::vector<int> only_indices(const std::map<int, float>& vs)
{
	std::vector<int> v(vs.size());
	std::transform(vs.begin(), vs.end(), v.begin(), [](auto a){
		return a.first;
	});
	return v;
}

bool analyzeGroup(
	int										group, 				// "our" group / the bone group we are currently analyzing
	std::map<int, std::map<int, float>>&	map,				// a map (vertex -> (group -> weight)) of all vertices (by index)
	std::vector<glm::vec3>&					vertices,			// positions (vertex -> position) of all vertices (index -> position)
	std::set<int>&						    visited,			// a list of all groups that have already been visited
	std::map<int, BoneInfo>&				out, 				// output
	int 									parent		= -1,	// our parent group
	glm::vec3								start		= {},	// joint of our parent and us
	glm::vec3								parentStart	= {}, 	// joint of our parent and its parent
	std::set<int> 							willVisit	= {})	// list of groups that will potentially be visisted by one of our anchestors, unless we visit them first
{
	std::cout << "Group " << group << " (parent " << parent << ")" << std::endl;
	BoneInfo& info = out[group];
	info.parent = parent;

	visited.insert(group);

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

	if(sharedCount == 0) // this analyzer cannot handle unconnected bones yet
	{
		info.head = center(allVertices, vertices)*1.01f;
		info.tail = center(allVertices, vertices)*0.99f;
		return false;
	}

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
		info.head = start;
		info.tail = avg;
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

		info.head = head;
		info.tail = tail;
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

			if(vs.size() < avgVertices*constants::groupDropout) // ignore groups under x% average size (e.g. with only a few vertices)
				continue;
			
			glm::vec3 c = center(vs, vertices);
			if(glm::length(c) > constants::brokenDistance) // avoid groups with broken distance
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

		info.head = head;
		info.tail = tail;
	}
	std::cout << std::endl;
	
	std::vector<std::pair<int, std::vector<int>>> others(otherGroups.size());
	std::copy(otherGroups.begin(), otherGroups.end(), others.begin());

	// decend into closer groups first (to avoid skipping B in A-B-C if we also share vertices with C)
	std::sort(others.begin(), others.end(), [mycenter, start, &vertices](auto& a, auto& b){
		auto c1 = center(a.second, vertices);
		auto c2 = center(b.second, vertices);
		return glm::distance(c1, mycenter) < glm::distance(c2, mycenter); // not sure why we don't use "start" instead of "mycenter", but I'm not gonna mess with this
	});

	std::set<int> willVisitAfter = willVisit; // we need to make a copy here, or (3) will always be true
	std::transform(others.begin(), others.end(), std::inserter(willVisitAfter, willVisitAfter.begin()), [](auto a){
		return a.first;
	});
	for(auto& [g, vs] : others)
	{
		if(g == group)
			continue;

		if(std::find(visited.begin(), visited.end(), g) == visited.end()) // don't visit groups again
		{
			glm::vec3 avg = center(vs, vertices);

			// skip decending if the group is a probable siblings
			bool probableSibiling = // (1) this is a probable sibling
				glm::distance(tail, avg) > glm::distance(start, avg) || // (a) detect siblings by checking if they are closer to our head than our tail
				glm::dot(tail-start, avg-tail) <= 0; // (b) detect going backwards with the dot product
			if(probableSibiling && // (1) this is a probable sibling
					parent != -1 && // (2) the root of the tree cannot have siblings
					std::find(willVisit.begin(), willVisit.end(), g) != willVisit.end()) // (3) a higher level group needs to "plan" to decend into the group or it is no sibling
				continue;
			
			if(analyzeGroup(g, map, vertices, visited, out, group, avg, start, willVisitAfter))
				info.childs.push_back(g);
		}
	}

	return true;
}

std::vector<std::pair<int, int>> decombine(int group, std::vector<std::map<int, int>>& mappings)
{
	std::vector<std::pair<int, int>> gs;
	for(int i=0; i<mappings.size(); i++)
	{
		for(auto [j, g] : mappings[i])
		{
			if(g == group)
			{
				gs.push_back({i, j});
			}
		}
	}
	return gs;
}

bool has_tree(nlohmann::json& json, int group)
{
	return std::any_of(json.begin(), json.end(), [group](auto a){
		return a["group"] == group;
	});
}

void buildJsons(
	int group, 
	std::map<int, BoneInfo>& groups, 
	std::vector<std::map<int, int>>& mappings, 
	std::vector<Mesh>& meshes,
	std::vector<nlohmann::json>& jsons,
	nlohmann::json& currentTree,
	std::vector<std::pair<int, int>>& visited,
	int currentMesh = -1) 
{
	std::cout << "Group " << group << ": ";
	auto gs = decombine(group, mappings);
	std::transform(gs.begin(), gs.end(), std::experimental::make_ostream_joiner(std::cout, ", "), [&meshes](auto a){
		return meshes[std::get<0>(a)].name + "[" + std::to_string(std::get<1>(a)) + "]";
	});
	if(currentMesh != -1)
		std::cout << " in " << meshes[currentMesh].name << std::endl;

	nlohmann::json* tree = &currentTree;
	if(currentMesh == -1)
	{
		currentMesh = std::get<0>(gs[0]);
		tree = &jsons[currentMesh].emplace_back(nlohmann::json::object());
	}

	BoneInfo& info = groups[group];
	int mygroup = -1;
	for(auto [m, g] : gs)
	{
		if(m == currentMesh)
		{
			mygroup = g;
			(*tree)["group"] = g;
			(*tree)["head"] = {info.head.x, info.head.y, info.head.z};
			(*tree)["tail"] = {info.tail.x, info.tail.y, info.tail.z};
		}
	}

	nlohmann::json& childs = (*tree)["children"] = nlohmann::json::array();

	for(int c : info.childs)
	{
		auto gsc = decombine(c, mappings);
		if(std::any_of(gsc.begin(), gsc.end(), [currentMesh](auto a){
			return std::get<0>(a) == currentMesh;
		}))
		{
			int gg = std::find_if(gsc.begin(), gsc.end(), [currentMesh](auto a){
						return std::get<0>(a) == currentMesh;
			})->second;
			if(!has_tree(jsons[currentMesh], gg) &&
				std::find(visited.begin(), visited.end(), std::pair<int, int>{currentMesh, gg}) == visited.end())
			{
				visited.push_back({currentMesh, gg});
				buildJsons(c, groups, mappings, meshes, jsons, childs.emplace_back(nlohmann::json::object()), visited, currentMesh);
			}
			for(auto [m, g] : gsc)
			{
				if(m != currentMesh && !has_tree(jsons[m], g) &&
					std::find(visited.begin(), visited.end(), std::pair<int, int>{m, g}) == visited.end())
				{
					nlohmann::json& child = jsons[m].emplace_back(nlohmann::json::object());
					child["parent"]["mesh"] = meshes[currentMesh].name;
					child["parent"]["bone"] = gg;
					child["group"] = g;
					child["head"] = {groups[c].head.x, groups[c].head.y, groups[c].head.z};
					child["tail"] = {groups[c].tail.x, groups[c].tail.y, groups[c].tail.z};
					child["children"] = nlohmann::json::array();

					visited.push_back({m, g});
				}
			}
		}
		else
		{
			for(auto [m, g] : gsc)
			{
				if(!has_tree(jsons[m], g) && std::find(visited.begin(), visited.end(), std::pair<int, int>{m, g}) == visited.end())
				{
					nlohmann::json& child = jsons[m].emplace_back(nlohmann::json::object());
					child["parent"]["mesh"] = meshes[currentMesh].name;
					child["parent"]["bone"] = mygroup;

					visited.push_back({m ,g});
					buildJsons(c, groups, mappings, meshes, jsons, child, visited, m);
				}
			}
		}
	}
}

int pick_next(const std::map<int, std::map<int, float>>& groupVertexWeight, const std::vector<glm::vec3>& allVertices, const std::set<int>& visited)
{
	int start;
	float minDistance = std::numeric_limits<float>::max();

	glm::vec3 supercenter = center(allVertices);
	// not the cleanest but it works I guess
	supercenter.x = 0;
	supercenter.z = 0;
	std::cout << "Super center: " << supercenter << std::endl;

	for(int i=0; i<groupVertexWeight.size(); i++)
	{
		if(visited.contains(i))
			continue;

		const std::map<int, float>& vs = groupVertexWeight.at(i);
		std::vector<int> v(vs.size());
		std::transform(vs.begin(), vs.end(), v.begin(), [](auto a){
			return a.first;
		});
		auto c = center(v, allVertices);

		float dist = glm::distance(c, supercenter);
		if(dist < minDistance)
		{
			start = i;
			minDistance = dist;
		}

		/*auto [min, max] = boundingBox(v, allVertices);
		float volume = (max.x-min.x) * (max.y-min.y) * (max.z-min.z);
		if(volume > maxVolume)
		{
			start = i;
			maxVolume = volume;
		}*/
	}
	return start;
}

int main(int argc, char* argv[])
{
	if((argc-2)%4 != 0)
	{
		std::cerr << "You must specify <name> <mesh-obj> <bone-json> <output-json> for each mesh part: argc = " << argc << std::endl;
		return 2;
	}
	std::vector<Mesh> meshes((argc-2)/4);
	for(int i=0; i<meshes.size(); i++)
	{
		Mesh& mesh = meshes[i];
		mesh.name		= argv[2 + 4*i + 0];
		mesh.meshObj 	= argv[2 + 4*i + 1];
		mesh.bonesJson 	= argv[2 + 4*i + 2];
		mesh.outputJson = argv[2 + 4*i + 3];
		std::cout << "Loading mesh " << mesh.name << std::endl;

		std::ifstream obj(mesh.meshObj);

		std::string line;
		while(std::getline(obj, line))
		{
			std::istringstream is(line);
			std::string type;
			is >> type;
			if(type=="v")
			{
				float x, y, z;
				is >> x >> y >> z;
				mesh.vertices.push_back({x, y, z});
			}
		}
		std::cout << "Loaded mesh from " << mesh.meshObj << ": " << mesh.vertices.size() << " vertices" << std::endl;

		std::ifstream bones(mesh.bonesJson);

		nlohmann::json json;
		bones >> json;
		std::map<std::string, std::map<std::string, float>> groupVertexWeight = json;
		for(auto& [name, vs] : groupVertexWeight)
		{
			int g = std::stoi(name);
			for(auto [vertex, weight] : vs)
			{
				if(weight < constants::minWeight)
					continue;

				int v = std::stoi(vertex);

				mesh.vertexGroupWeight[v][g] = weight;
				mesh.groupVertexWeight[g][v] = weight;
			}
		}
		std::cout << "Loaded bones from " << mesh.bonesJson << ": " << mesh.groupVertexWeight.size() << " bone groups" << std::endl;
		std::cout << std::endl;
	}
	std::cout << "Loaded " << meshes.size() << " mesh(es)" << std::endl;

	nlohmann::json referencePose;
	{
		std::ifstream in(argv[1]);
		in >> referencePose;
	}
	std::cout << "Loaded reference pose" << std::endl;

	std::map<std::tuple<int, int>, std::tuple<int, int>> sharedBoneGroups;
	for(int i=0; i<meshes.size(); i++)
	{
		Mesh& a = meshes[i];
		for(int j=i+1; j<meshes.size(); j++)
		{
			Mesh& b = meshes[j];
			
			for(auto& [g1, p1] : referencePose[a.name].items())
			{
				for(auto& [g2, p2] : referencePose[b.name].items())
				{
					if(p1 == p2)
					{
						if(!a.groupVertexWeight.contains(std::stoi(g1)))
							continue;
						if(!b.groupVertexWeight.contains(std::stoi(g2)))
							continue;

						std::cout << "Found shared group: " << a.name << "[" << g1 << "] = " << b.name << "[" << g2 << "]" << std::endl;
						sharedBoneGroups[{j, std::stoi((std::string)g2)}] = {i, std::stoi((std::string)g1)};
						break;
					}
				}
			}
		}
	}

	int vertexSum = std::accumulate(meshes.begin(), meshes.end(), 0, [](int a, Mesh& b){
		return a + b.vertices.size();
	});

	std::vector<glm::vec3> allVertices(vertexSum);
	std::vector<std::map<int, int>> boneGroupMappings(meshes.size());
	std::map<int, std::map<int, float>> groupVertexWeight;
	std::map<int, std::map<int, float>> vertexGroupWeight;

	int vertexOffset = 0;
	int nextGroup = 0;
	for(int i=0; i<meshes.size(); i++)
	{
		Mesh& mesh = meshes[i];
		std::copy(mesh.vertices.begin(), mesh.vertices.end(), allVertices.begin()+vertexOffset);

		std::map<int, int>& boneMappings = boneGroupMappings[i];
		for(auto& [g, vs] : mesh.groupVertexWeight)
		{
			std::tuple<int, int> t = {i, g};
			int mapping;
			if(sharedBoneGroups.contains(t))
			{
				auto t2 = sharedBoneGroups[t];
				mapping = boneMappings[g] = boneGroupMappings[std::get<0>(t2)][std::get<1>(t2)];
			}
			else
			{
				mapping = boneMappings[g] = nextGroup++;
			}
			std::cout << mesh.name << ", " << g << " -> " << mapping << std::endl;

			std::map<int, float>& inner = groupVertexWeight[mapping];
			std::transform(vs.begin(), vs.end(), std::inserter(inner, inner.end()), [vertexOffset](std::pair<int, float> a){
				return std::pair<int, float>{std::get<0>(a)+vertexOffset, std::get<1>(a)};
			});
		}
		for(auto& [v, gs] : mesh.vertexGroupWeight)
		{
			std::map<int, float>& inner = vertexGroupWeight[v+vertexOffset];
			std::transform(gs.begin(), gs.end(), std::inserter(inner, inner.end()), [&boneMappings](std::pair<int, float> a){
				return std::pair<int, float>{boneMappings[std::get<0>(a)], std::get<1>(a)};
			});
		}

		vertexOffset += mesh.vertices.size();
	}
	std::cout << "Total vertices: " << vertexSum << " or " << vertexGroupWeight.size() << std::endl;
	std::cout << "Total bone groups: " << nextGroup << " or " << groupVertexWeight.size() << std::endl;

	std::set<int> visited;
	std::map<int, BoneInfo> out;
	std::set<int> starts;
	while(visited.size() < groupVertexWeight.size())
	{
		int start = pick_next(groupVertexWeight, allVertices, visited);

		std::cout << "Starting at group " << start << std::endl;
		starts.insert(start);

		glm::vec3 c;
		{
			const std::map<int, float>& vs = groupVertexWeight.at(start);
			std::vector<int> v(vs.size());
			std::transform(vs.begin(), vs.end(), v.begin(), [](auto a){
				return a.first;
			});
			c = center(v, allVertices);
		}
		analyzeGroup(start, vertexGroupWeight, allVertices, visited, out, -1, c);
	}

	std::cout << visited.size() << " / " << groupVertexWeight.size() << std::endl; 

	std::vector<nlohmann::json> jsons(meshes.size());
	std::fill(jsons.begin(), jsons.end(), nlohmann::json::array());

	nlohmann::json j;
	std::vector<std::pair<int, int>> visited2;
	for(auto start : starts)
	{
		buildJsons(start, out, boneGroupMappings, meshes, jsons, j, visited2);
	}
	for(int i=0; i<meshes.size(); i++)
	{
		std::ofstream o(meshes[i].outputJson);
		nlohmann::json jo{};
		jo["trees"] = jsons[i];
		o << std::setw(4) << jo << std::endl;
	}
	std::cout << std::setw(4) << j << std::endl;
}
