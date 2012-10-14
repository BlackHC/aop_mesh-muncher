#pragma once

#include <mathUtility.h>
#include <grid.h>

#include "optixProgramInterface.h"

namespace ProbeGenerator {
	/* Ideas:
	 * could generate probes using Morton indexing, which should speed up the tracing because similar probes will always be next to each other
	 */
	typedef OptixProgramInterface::Probe Probe;

	void initDirections();
	void transformProbe( const Probe &probe, const Obb::Transformation &transformation, Probe &transformedProbe );
	void transformProbes( const std::vector<Probe> &probes,const Obb::Transformation &transformation, std::vector<Probe> &transformedProbes );

	void generateRegularInstanceProbes( const Eigen::Vector3f &size, const float resolution, std::vector<Probe> &probes );
	void generateQueryProbes( const Obb &obb, const float resolution, std::vector<Probe> &transformedProbes );

	void appendProbesFromSample( const Eigen::Vector3f &position, const Eigen::Vector3f &averagedNormal, std::vector< Probe > &probes );
};