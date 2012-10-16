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

void visualizeProbeDataset( float resolution, const std::vector< SortedProbeDataset::Probe > &probes, const std::vector< SortedProbeDataset::ProbeContext > &probeContexts, ProbeVisualizationMode pvm );