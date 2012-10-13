#pragma once

//#include <Eigen/Eigen>
#include <vector>
#include <map>

#include "sgsInterface.h"

struct ModelDatabase {
	struct IdInformation {
		std::string name;
		std::string shortName;

		float volume;
		float area;
		float diagonalLength;

		typedef std::vector< SGSInterface::Probe > Probes;
		Probes probes;
		VoxelizedModel::Voxels voxels;

		// TODO: add move semantics [10/13/2012 kirschan2]
	};

	std::vector< IdInformation > informationById;

	// see candidateFinderCache.cpp
	bool load( const char *filename );
	void store( const char *filename );
};