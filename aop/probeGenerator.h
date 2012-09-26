#pragma once

#include <mathUtility.h>
#include <grid.h>

#include "optixProgramInterface.h"

// optix eigen wrapper
Eigen::Vector3f::MapType EigenMap( optix::float3 &v ) {
	return Eigen::Vector3f::Map( &v.x );
}

Eigen::Vector3f::ConstMapType EigenMap( const optix::float3 &v ) {
	return Eigen::Vector3f::Map( &v.x );
}

namespace ProbeGenerator {
	/* Ideas:
	 * could generate probes using Morton indexing, which should speed up the tracing because similar probes will always be next to each other
	 */
	typedef OptixProgramInterface::Probe Probe;

	void initDirections();
	void transformProbe( const Probe &probe, const OBB::Transformation &transformation, Probe &transformedProbe );
	void transformProbes( const std::vector<Probe> &probes,const OBB::Transformation &transformation,  std::vector<Probe> &transformedProbes );
	void generateInstanceProbes( const Eigen::Vector3f &size, const float resolution, std::vector<Probe> &probes );
	void generateQueryProbes( const OBB &obb, const float resolution, std::vector<Probe> &transformedProbes );
};