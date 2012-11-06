#pragma once

#include <vector>
#include "sgsInterface.h"
#include "probeDatabase.h"

void visualizeProbes( float resolution, const ProbeContext::RawProbes &probes );

enum GridVisualizationMode {
	GVM_POSITION,
	GVM_HITS,
	GVM_NORMAL,
	GVM_MAX
};

void visualizeColorGrid(
	const VoxelizedModel::Voxels &grid,
	GridVisualizationMode gvm = GVM_POSITION
);

enum ProbeVisualizationMode {
	PVM_COLOR,
	PVM_OCCLUSION,
	PVM_DISTANCE,
	PVM_MAX
};

void visualizeProbeDataset(
	const Eigen::Vector3f &skyColor,
	float maxResolution,
	float gridResolution,
	float scaleFactor,
	const ProbeContext::DBProbes &probes,
	const ProbeContext::DBProbeSamples &probeSamples,
	ProbeVisualizationMode pvm
);

void visualizeRawProbeSamples(
	const Eigen::Vector3f &missColor,
	float maxDistance,
	float gridResolution,
	float scaleFactor,
	const ProbeContext::RawProbes &probes,
	const ProbeContext::RawProbeSamples &probeSamples,
	ProbeVisualizationMode pvm
);