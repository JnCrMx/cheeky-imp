#include <assert.h>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <nlohmann/json.hpp>

struct pose_part
{
	std::string name;
	int offset;
};

int main(int argc, char *argv[])
{
	assert(argc > 3);

	std::string input_path = argv[1];
	std::string output_path = argv[2];

	std::vector<pose_part> parts(argc-3);
	for(int i=0; i<argc-3; i++)
	{
		std::string line = argv[i+3];

		std::istringstream is_line(line);

		std::string key;
		std::string value;
		if(std::getline(is_line, key, '=') && std::getline(is_line, value, '='))
		{
			int val = std::stoi(value, nullptr, 0);
			parts[i] = {key, val};
		}
	}

	boost::interprocess::file_mapping map(input_path.c_str(), boost::interprocess::read_only);
	boost::interprocess::mapped_region region(map, boost::interprocess::read_only);
	glm::mat3x4* matrices = (glm::mat3x4*) region.get_address();

	std::map<std::string, std::map<std::string, std::array<float, 16>>> groupMap;

	for(pose_part part : parts)
	{
		glm::mat3x4* base = matrices + part.offset;
		for(int i = 0; i < 256; i++)
		{
			glm::mat3x4 mat = base[i];
			glm::mat4 mat2(mat);

			mat2[3][0] = mat2[0][3];
			mat2[3][1] = mat2[1][3];
			mat2[3][2] = mat2[2][3];
			mat2[3][3] = 0.0;

			mat2 = glm::transpose(mat2);

			std::array<float, 16> floats;

			const float* source = glm::value_ptr(mat2);
			for(int i=0; i<16; i++)
				floats[i] = source[i];

			groupMap[part.name][std::to_string(i)] = floats;
		}
	}
	nlohmann::json j = groupMap;
	std::ofstream outJson(output_path);
	outJson << j;
}
