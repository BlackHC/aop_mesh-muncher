#pragma once

#include <vector>
#include "sgsInterface.h"
#include "probeDatabase.h"

void visualizeProbes( float resolution, const std::vector< SGSInterface::Probe > &probes );

enum GridVisualizationMode {
	GVM_POSITION,
	GVM_HITS,
	GVM_NORMAL,
	GVM_MAX
};

void visualizeColorGrid( const VoxelizedModel::Voxels &grid, GridVisualizationMode gvm = GVM_POSITION );

enum ProbeVisualizationMode {
	PVM_COLOR,
	PVM_OCCLUSION,
	PVM_DISTANCE,
	PVM_MAX
};

void visualizeProbeDataset(
	const Eigen::Vector3f &skyColor,
	float maxResolution,
	float resolution,
	const DBProbes &probes,
	const DBProbeContexts &probeContexts,
	ProbeVisualizationMode pvm
);