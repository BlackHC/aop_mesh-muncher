#include "Core/inc/Core.h"
#include "Core/inc/Exception.h"
#include "Core/inc/Log.h"
#include "Core/inc/nString.h"

#include "niven.Volume.FileBlockStorage.h"
#include "niven.Volume.MarchingCubes.h"
#include "niven.Volume.Volume.h"

#include "niven.Core.Iterator3D.h"

#include <iostream>
#include <functional>

#include "mipVolume.h"
#include "cache.h"

#include "memoryBlockStorage.h"

#include "gtest.h"

#include "findDistances.h"

using namespace niven;

void findNearestPointFromCandidateVoxels( DenseCache &cache, const std::vector<Vector3i> &candidates, const Vector3i &refPosition, const int level, int /*INOUT*/ &minDistanceSquared, Vector3i /*OUT*/ *nearestPoint ) {
	for( int i = 0 ; i < candidates.size() ; i++ ) {
		Vector3i minCube, maxCube;
		cache.volume.GetCubeForMippedVoxel( level, candidates[i], minCube, maxCube );
		
		const int distanceSquared = squaredDistanceAABoxPoint( minCube, maxCube, refPosition );
		if( distanceSquared < minDistanceSquared ) {
			minDistanceSquared = distanceSquared;
			if( nearestPoint ) {
				*nearestPoint = nearestPointOnAABoxToPoint( minCube, maxCube, refPosition );
			}
		}
	}		 
}

void filterCandidates( DenseCache &cache, int level, const Vector3i &refPosition, const std::vector<Vector3i> &candidates, std::vector<Vector3i> &nearestCells ) {
	int nearestMaxDistanceSquared = INT_MAX;

	for( int i = 0 ; i < candidates.size() ; i++ ) {
		const Vector3i &voxelPosition = candidates[i];

		// mipmap cube
		Vector3i minCube, maxCube;
		cache.volume.GetCubeForMippedVoxel( level, voxelPosition, minCube, maxCube );

		int maxDistanceSquared = squaredMaxDistanceAABoxPoint( minCube, maxCube, refPosition );
		nearestMaxDistanceSquared = std::min( nearestMaxDistanceSquared, maxDistanceSquared );
	}

	nearestCells.clear();
	for( int i = 0 ; i < candidates.size() ; i++ ) {
		const Vector3i &voxelPosition = candidates[i];

		// mipmap cube
		Vector3i minCube, maxCube;
		cache.volume.GetCubeForMippedVoxel( level, voxelPosition, minCube, maxCube );

		int minDistanceSquared = squaredDistanceAABoxPoint( minCube, maxCube, refPosition );

		if( minDistanceSquared <= nearestMaxDistanceSquared ) {
			nearestCells.push_back( voxelPosition );
		}
	}
}

int findSquaredDistanceToNearestVoxel( DenseCache &cache, const Vector3i &refPosition, int minLevel, Vector3i *nearestPoint ) {
	std::vector<Vector3i> currentLevel, candidates;

	int level = cache.volume.levels.size() - 1;
	// handle the upper-most level (1x1)
	{		
		Vector3i voxelPosition(0,0,0);
		if( cache.GetVoxel( level, voxelPosition ) > 0 ) {
			currentLevel.push_back( voxelPosition );
		}
	}
	while( !currentLevel.empty() ) {
		level--;

		candidates.clear();
		for( int i = 0 ; i < currentLevel.size() ; i++ ) {
			const Vector3i &voxelPosition = currentLevel[i];

			// assert: voxel is not empty			

			// recurse
			for( int i = 0 ; i < 8 ; i++ ) {
				Vector3i subVoxelPosition = voxelPosition * 2 + indexToCubeCorner[ i ];

				if( cache.GetVoxel( level, subVoxelPosition ) > 0 ) {
					candidates.push_back( subVoxelPosition );
				}
			}
		}

		if( level == minLevel ) {
			break;
		}

		// only continue with the best candidates
		filterCandidates( cache, level, refPosition, candidates, currentLevel );
	}

	if( !candidates.empty() && level == minLevel ) {
		int minDistanceSquared = INT_MAX;
		findNearestPointFromCandidateVoxels( cache, candidates, refPosition, level, minDistanceSquared, nearestPoint );
		return minDistanceSquared;
	}
	return INT_MAX;
}

void filterConditionedCandidates( DenseCache &cache, int level, const Vector3i &refPosition, const std::vector<Vector3i> &candidates, std::vector<Vector3i> &nearestCells, const std::vector<Vector3i> &partialCandidates, std::vector<Vector3i> &partialNearestCells ) {
	int nearestMaxDistanceSquared = INT_MAX;

	// only full candidates count for determining maxDistance 
	for( int i = 0 ; i < candidates.size() ; i++ ) {
		const Vector3i &voxelPosition = candidates[i];

		// mipmap cube
		Vector3i minCube, maxCube;
		cache.volume.GetCubeForMippedVoxel( level, voxelPosition, minCube, maxCube );

		int maxDistanceSquared = squaredMaxDistanceAABoxPoint( minCube, maxCube, refPosition );
		nearestMaxDistanceSquared = std::min( nearestMaxDistanceSquared, maxDistanceSquared );
	}

	nearestCells.clear();
	for( int i = 0 ; i < candidates.size() ; i++ ) {
		const Vector3i &voxelPosition = candidates[i];

		// mipmap cube
		Vector3i minCube, maxCube;
		cache.volume.GetCubeForMippedVoxel( level, voxelPosition, minCube, maxCube );

		int minDistanceSquared = squaredDistanceAABoxPoint( minCube, maxCube, refPosition );
		if( minDistanceSquared <= nearestMaxDistanceSquared ) {
			nearestCells.push_back( voxelPosition );
		}
	}

	partialNearestCells.clear();
	for( int i = 0 ; i < partialCandidates.size() ; i++ ) {
		const Vector3i &voxelPosition = partialCandidates[i];

		// mipmap cube
		Vector3i minCube, maxCube;
		cache.volume.GetCubeForMippedVoxel( level, voxelPosition, minCube, maxCube );

		int minDistanceSquared = squaredDistanceAABoxPoint( minCube, maxCube, refPosition );
		if( minDistanceSquared <= nearestMaxDistanceSquared ) {
			partialNearestCells.push_back( voxelPosition );
		}
	}
}

int findSquaredDistanceToNearestConditionedVoxel( DenseCache &cache, const Vector3i &refPosition, const std::function<ConditionedVoxelType (const Vector3i &min, const Vector3i &max)> &conditioner, int minLevel, Vector3i *nearestPoint ) {
	std::vector<Vector3i> currentLevel, candidates;
	std::vector<Vector3i> currentLevelPartials, partialCandidates;

	int level = cache.volume.levels.size() - 1;
	// handle the upper-most level (1x1)
	{		
		Vector3i voxelPosition(0,0,0);
		if( cache.GetVoxel( level, voxelPosition ) > 0 ) {
			currentLevelPartials.push_back( voxelPosition );
		}
	}
	while( !currentLevel.empty() || !currentLevelPartials.empty() ) {
		level--;

		// candidate handling
		candidates.clear();
		for( int i = 0 ; i < currentLevel.size() ; i++ ) {
			const Vector3i &voxelPosition = currentLevel[i];

			// assert: voxel is not empty			

			// recurse
			for( int i = 0 ; i < 8 ; i++ ) {
				Vector3i subVoxelPosition = voxelPosition * 2 + indexToCubeCorner[ i ];

				if( cache.GetVoxel( level, subVoxelPosition ) > 0 ) {
					candidates.push_back( subVoxelPosition );
				}
			}
		}

		// partial candidate handling
		partialCandidates.clear();
		for( int i = 0 ; i < currentLevelPartials.size() ; i++ ) {
			const Vector3i &voxelPosition = currentLevelPartials[i];

			// assert: voxel is not empty			

			// recurse
			for( int i = 0 ; i < 8 ; i++ ) {
				Vector3i subVoxelPosition = voxelPosition * 2 + indexToCubeCorner[ i ];

				if( cache.GetVoxel( level, subVoxelPosition ) > 0 ) {
					Vector3i minCube, maxCube;
					cache.volume.GetCubeForMippedVoxel( level, subVoxelPosition, minCube, maxCube );

					ConditionedVoxelType type = conditioner( minCube, maxCube );
					switch( type ) {
					case CVT_NO_MATCH:
						// ignore
						break;
					case CVT_PARTIAL:
						partialCandidates.push_back( subVoxelPosition );
						break;
					case CVT_MATCH:
						candidates.push_back( subVoxelPosition );
						break;
					}
				}
			}
		}

		if( level > minLevel ) {
			// only continue with the best candidates
			filterConditionedCandidates( cache, level, refPosition, candidates, currentLevel, partialCandidates, currentLevelPartials );
		}
		else {
			// reached minLevel -> calculate result
			int minDistanceSquared = INT_MAX;
			findNearestPointFromCandidateVoxels( cache, candidates, refPosition, level, minDistanceSquared, nearestPoint );
			findNearestPointFromCandidateVoxels( cache, partialCandidates, refPosition, level, minDistanceSquared, nearestPoint );
			return minDistanceSquared;
		}
	}

	return INT_MAX;
}

/*
TODO:

query multiple distances at once using multiple conditioners
either by using totally separate arrays and being more efficient by using fewer block queries, or
by using the same candidate buffers at first and only filtering differently and splitting the results in the end
*/