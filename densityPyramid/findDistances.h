#pragma once

#include "cache.h"

enum ConditionedVoxelType {
	CVT_NO_MATCH = 0,
	CVT_PARTIAL = 1,
	CVT_MATCH = 2
};

inline ConditionedVoxelType operator &&( const ConditionedVoxelType a, const ConditionedVoxelType b ) {
	return ConditionedVoxelType( std::min( a, b ) );
}

inline ConditionedVoxelType operator ||( const ConditionedVoxelType a, const ConditionedVoxelType b ) {
	return ConditionedVoxelType( std::max( a, b ) );
}

inline ConditionedVoxelType operator !(const ConditionedVoxelType a) {
	return ConditionedVoxelType( CVT_MATCH - a );
}

int findSquaredDistanceToNearestVoxel( niven::DenseCache &cache, const niven::Vector3i &refPosition, int minLevel = 0, niven::Vector3i *nearestPoint = nullptr );
int findSquaredDistanceToNearestConditionedVoxel( niven::DenseCache &cache, const niven::Vector3i &refPosition, const std::function<ConditionedVoxelType (const niven::Vector3i &min, const niven::Vector3i &max)> &conditioner, int minLevel = 0, niven::Vector3i *nearestPoint = nullptr );