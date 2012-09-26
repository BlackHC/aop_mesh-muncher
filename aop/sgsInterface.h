#pragma once

#include "optixProgramInterface.h"
#include "sgsSceneRenderer.h"

namespace SGSInterface {
	typedef OptixProgramInterface::Probe Probe;

	void generateProbes( int instanceIndex, float resolution, SGSSceneRenderer &renderer, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes );
}