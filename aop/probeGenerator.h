#pragma once

#include <mathUtility.h>
#include <grid.h>

#include "optixProgramInterface.h"

#include <vector>

namespace ProbeGenerator {
	using OptixProgramInterface::TransformedProbe;
	using OptixProgramInterface::TransformedProbes;

	/* Ideas:
	 * could generate probes using Morton indexing, which should speed up the tracing because similar probes will always be next to each other
	 */
	typedef Eigen::Matrix< signed char, 3, 1 > char3;

	struct Probe {
		char3 position;
		unsigned char directionIndex;
	};
	typedef std::vector< Probe > Probes;

	void initDirections();

	const Eigen::Vector3f &getDirection( int index );
	const Eigen::Vector3f *getDirections();
	int getNumDirections();

	void initOrientations();

	int getNumOrientations();
	const int *getRotatedDirections( int orientationIndex );
	const Eigen::Matrix3f getRotation( int orientationIndex );

	Eigen::Vector3i getGridHalfExtent( const Eigen::Vector3f &size, const float resolution );

	void transformProbe(
		const Probe &probe,
		const Obb::Transformation &transformation,
		const float resolution,
		TransformedProbe &transformedProbe
	);
	void transformProbes(
		const std::vector<Probe> &probes,
		const Obb::Transformation &transformation,
		const float resolution,
		TransformedProbes &transformedProbes
	);

	void generateRegularInstanceProbes(
		const Eigen::Vector3f &size,
		const float resolution,
		Probes &probes
	);
	void generateQueryProbes(
		const Eigen::Vector3f &size,
		const float resolution,
		Probes &probes
	);

	void appendProbesFromSample(
		const float resolution,
		const Eigen::Vector3f &position,
		const Eigen::Vector3f &averagedNormal,
		Probes &probes
	);
};