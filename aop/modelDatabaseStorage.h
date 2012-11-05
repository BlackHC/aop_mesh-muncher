#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "modelDatabase.h"

/*
namespace Serializer {
		template< typename Reader >
		void read( Reader &reader, type &value ) {
			
		}
		template< typename Writer > \
		void write( Writer &writer, const type &value ) {
			
		}
	}*/

SERIALIZER_DEFAULT_EXTERN_IMPL( IndexMapping3<>, (size)(count)(indexToPosition)(positionToIndex) )
SERIALIZER_DEFAULT_EXTERN_IMPL( VoxelizedModel::Voxels, (mapping)(data) )
SERIALIZER_DEFAULT_EXTERN_IMPL( ModelDatabase::ModelInformation, (name)(shortName)(volume)(area)(diagonalLength)(probes)(voxels) )

SERIALIZER_ENABLE_RAW_MODE_EXTERN( ProbeGenerator::Probe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( VoxelizedModel::NormalOverdraw4ub );