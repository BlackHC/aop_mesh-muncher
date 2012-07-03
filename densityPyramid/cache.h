#pragma once

#include "mipVolume.h"

#include <vector>

namespace niven {
struct DenseCache {
	MipVolume &volume;

	struct CacheEntry {
		bool cached;
		std::vector<uint16> data;

		CacheEntry() : cached( false ) {}
	};

	bool empty;
	std::vector< CacheEntry > cacheEntries;
	std::vector< CacheEntry* > levelCache;

	DenseCache( MipVolume &volume ) : volume( volume ) {
		Init();
	}

	void Init() {
		int numBlocks = 0;
		for( int i = 0 ; i < volume.levels.size() ; i++ ) {
			const MipVolume::LevelInfo &levelInfo = volume.levels[i];
			numBlocks += levelInfo.size.X() * levelInfo.size.Y() * levelInfo.size.Z();
		}
		empty = numBlocks == 0;
		if( empty ) {
			cacheEntries.resize(1);
			return;
		}

		cacheEntries.resize( numBlocks );
		levelCache.resize( volume.levels.size() );
		
		int blockIndex = 0;
		for( int i = 0 ; i < volume.levels.size() ; i++ ) {
			levelCache[i] = &cacheEntries.front() + blockIndex;

			const MipVolume::LevelInfo &levelInfo = volume.levels[i];
			blockIndex += levelInfo.size.X() * levelInfo.size.Y() * levelInfo.size.Z();
		}
	}

	const std::vector<uint16> & GetBlock( int level, const Vector3i &position ) {
		if( empty ) {
			return cacheEntries[0].data;
		}

		const MipVolume::LevelInfo &levelInfo = volume.levels[level];
		int index = position.X() + position.Y() * levelInfo.size.X() + position.Z() * (levelInfo.size.X() * levelInfo.size.Y());
		CacheEntry &cacheEntry = levelCache[level][index];
		if( cacheEntry.cached ) {
			return cacheEntry.data;
		}

		cacheEntry.cached = true;
		if( volume.HasBlock( level, position ) ) {
			cacheEntry.data.resize( volume.blockVoxelCount );
			volume.GetBlock( level, position, cacheEntry.data );
		}
		return cacheEntry.data;
	}

	uint16 GetVoxel( int level, const Vector3i &voxelPosition ) {
		Vector3i blockIndex, localPosition;
		volume.SplitCoordinates( voxelPosition, blockIndex, localPosition );

		const std::vector<uint16> &blockData = GetBlock( level, blockIndex );
		if( blockData.empty() ) {
			return 0;
		}
		return volume.GetVoxel( blockData, localPosition );
	}
};
}