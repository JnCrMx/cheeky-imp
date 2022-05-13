#include <assert.h>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <nlohmann/json.hpp>

struct pose_part
{
	std::string name;
	std::string file;
	int offset;
};

struct options
{
	bool transpose = false;
};

int main(int argc, char *argv[])
{
	assert(argc > 3);

	std::string input_path = argv[1];
	std::string output_path = argv[2];

	options my_options = {};
	int partsOffset;
	for(partsOffset = 3; std::string(argv[partsOffset]).starts_with("--"); partsOffset++)
	{
		std::string option = std::string(argv[partsOffset]).substr(2);
		std::cout << "Enabling option " << option << ".\n";

		if(option == "transpose")
			my_options.transpose = true;
		else
			std::cerr << "Unknown option: --" << option << std::endl;
	}

	std::vector<pose_part> parts(argc-partsOffset);
	for(int i=0; i<argc-partsOffset; i++)
	{
		std::string line = argv[i+partsOffset];
		std::cout << "Using part " << line << ".\n";

		std::istringstream is_line(line);

		std::string key;
		std::string file;
		std::string offset;
		if(std::getline(is_line, key, '=') && std::getline(is_line, file, ':') && std::getline(is_line, offset))
		{
			int o = std::stoi(offset, nullptr, 0);
			parts[i] = {key, file, o};
		}
		else
		{
			std::cerr << "Couldn't read part: " << line << std::endl;
		}
	}

	std::map<std::string, std::map<std::string, std::array<float, 16>>> groupMap;

	for(pose_part part : parts)
	{
		boost::interprocess::file_mapping map((input_path+"/"+part.file).c_str(), boost::interprocess::read_only);
		boost::interprocess::mapped_region region(map, boost::interprocess::read_only);
		glm::mat3x4* matrices = (glm::mat3x4*) region.get_address();

		glm::mat3x4* base = matrices + part.offset;
		for(int i = 0; i < 256; i++)
		{
			glm::mat3x4 mat = base[i];
			glm::mat4 mat2(mat);

			mat2[3][0] = mat2[0][3];
			mat2[3][1] = mat2[1][3];
			mat2[3][2] = mat2[2][3];
			mat2[3][3] = 0.0;

			if(my_options.transpose)
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
	outJson << std::setw(4) << j;
}
