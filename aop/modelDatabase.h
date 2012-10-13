#pragma once

//#include <Eigen/Eigen>
#include <vector>
#include <map>

#include "sgsInterface.h"

struct ModelDatabase {
	struct IdInformation {
		float volume;
		float area;
		float diagonalLength;

		typedef std::vector< SGSInterface::Probe > Probes;
		std::map< float, Probes > probes;

		// TODO: add move semantics [10/13/2012 kirschan2]
	};

	std::vector< IdInformation > informationById;
};