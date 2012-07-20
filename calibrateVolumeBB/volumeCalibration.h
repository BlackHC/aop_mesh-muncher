#pragma once

#include "niven.Core.Math.Vector.h"
#include "niven.Core.ArrayRef.h"
#include "niven.Volume.BlockStorage.h"
#include "niven.Core.String.h"


struct VolumeCalibration {
	niven::Vector3f offset;
	float blockSize;

	void readFrom( const niven::Volume::IBlockStorage &volume ) {
		volume.GetAttribute( "offset", niven::MutableArrayRef<niven::Vector3f>( offset ) );
		volume.GetAttribute( "blockSize", niven::MutableArrayRef<float>( blockSize ) );
	}

	niven::Vector3f getPosition( const niven::Vector3i &blockIndex ) const {
		return offset + blockIndex.Cast<float>() * blockSize;
	}

	niven::Vector3i getBlockIndex( const niven::Vector3f &position ) const {
		return ((position - offset) / blockSize).Cast<int>();
	}

	niven::Vector3f getSize( const niven::Vector3i &indexSize ) const {
		return indexSize.Cast<float>() * blockSize;
	}
};

struct LayerCalibration {
	const VolumeCalibration *volumeCalibration;
	int blockResolution;

	void readFrom( const niven::Volume::IBlockStorage &volume, const VolumeCalibration &calibration, const char *layerName ) {
		auto descriptor = volume.GetLayerDescriptor( layerName );
		blockResolution = descriptor.blockResolution;
		volumeCalibration = &calibration;
	}

	// TODO: rename to toPosition etc!
	niven::Vector3f getPosition( const niven::Vector3i &globalIndex ) const {
		return volumeCalibration->offset + globalIndex.Cast<float>() * volumeCalibration->blockSize / blockResolution;
	}

	niven::Vector3f getPosition( const niven::Vector3i &blockIndex, const niven::Vector3i &cellIndex ) const {
		return getPosition( toGlobalIndex( blockIndex, cellIndex ) );
	}

	niven::Vector3f getGlobalIndex( const niven::Vector3f &position ) const {
		return ((position - volumeCalibration->offset) * blockResolution / volumeCalibration->blockSize);
	}

	niven::Vector3i getGlobalFloorIndex( const niven::Vector3f &position ) const {
		return VectorFloor( getGlobalIndex( position ) ).Cast<int>();
	}

	niven::Vector3i getGlobalCeilIndex( const niven::Vector3f &position ) const {
		return VectorCeil( getGlobalIndex( position ) ).Cast<int>();
	}

	void splitGlobalIndex( const niven::Vector3i &globalIndex, niven::Vector3i &blockIndex, niven::Vector3i &cellIndex ) const {
		blockIndex = globalIndex / blockResolution;
		cellIndex = globalIndex - blockIndex * blockResolution;
	}

	niven::Vector3i toGlobalIndex( const niven::Vector3i &blockIndex, const niven::Vector3i &cellIndex ) const {
		return blockIndex * blockResolution + cellIndex;
	}

	niven::Vector3f getSize( const niven::Vector3i &indexSize ) const {
		return indexSize.Cast<float>() * volumeCalibration->blockSize / blockResolution;
	}
};