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

	niven::Vector3f getPosition( const niven::Vector3i &blockIndex, const niven::Vector3i &cellPosition ) const {
		return volumeCalibration->getPosition( blockIndex ) + cellPosition.Cast<float>() * volumeCalibration->blockSize / blockResolution;
	}

	niven::Vector3f getPosition( const niven::Vector3i &globalIndex ) const {
		return volumeCalibration->offset + globalIndex.Cast<float>() * volumeCalibration->blockSize / blockResolution;
	}

	niven::Vector3f getSize( const niven::Vector3i &indexSize ) const {
		return indexSize.Cast<float>() * volumeCalibration->blockSize / blockResolution;
	}
};