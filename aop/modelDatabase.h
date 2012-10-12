#pragma once

//#include <Eigen/Eigen>
#include <vector>

struct ModelDatabase {
	struct IdInformation {
		float volume;
		float area;
		float diagonalLength;
	};

	std::vector< IdInformation > informationById;
};