/**
* @author: Matthias Reitinger
*
* License: 3c-BSD
*/

#include "Core/inc/Core.h"
#include "Core/inc/CommandlineParser.h"
#include "Core/inc/Exception.h"
#include "Core/inc/Log.h"
#include "Core/inc/nString.h"
#include "Core/inc/StringFormat.h"
#include "Core/inc/io/File.h"
#include "Core/inc/io/FileSystem.h"

#include "Engine/inc/BaseApplication3D.h"
#include "niven.Render.DrawCommand.h"
#include "niven.Render.RenderBuffer.h"
#include "niven.Render.IndexBuffer.h"
#include "niven.Render.VertexBuffer.h"
#include "niven.Render.RenderContext.h"
#include "niven.Render.RenderSystem.h"
#include "niven.Render.RenderTargetDescriptor.h"
#include "niven.Render.RenderTargetTexture3D.h"
#include "niven.Render.Effect.h"
#include "niven.Render.EffectLoader.h"
#include "niven.Render.EffectManager.h"
#include "niven.Render.StreamOutQuery.h"
#include "niven.Render.Texture3D.h"
#include "niven.Render.VertexFormats.PositionNormal.h"
#include "niven.Render.VertexLayout.h"
#include "niven.Volume.FileBlockStorage.h"
#include "niven.Volume.MarchingCubes.h"
#include "niven.Volume.Volume.h"

#include "niven.Core.MemoryLayout.h"
#include "niven.Core.Iterator3D.h"

#include <iostream>
#include <queue>
#include <functional>

using namespace niven;

// assuming even blockResolution

struct VolumeCoordinates {
	Volume::IBlockStorage &container;
	
	MemoryLayout3D layout;
	int blockResolution;
	int borderSize;
	int totalSize;
	int blockVoxelCount;

	struct LevelInfo {
		Vector3i min, size;
		int voxelSize;
	};

	std::vector<LevelInfo> levels;

	VolumeCoordinates( Volume::IBlockStorage &container ) : container( container ) {}

	void Init() {
		Volume::FileBlockStorage::LayerDescriptor layerDescriptor = container.GetLayerDescriptor( "Density" );

		blockResolution = layerDescriptor.blockResolution;
		borderSize = layerDescriptor.borderSize;
		totalSize = blockResolution + borderSize*2;
		blockVoxelCount = totalSize * totalSize * totalSize;

		layout = MemoryLayout3D( totalSize );

		Volume::IBlockStorage::BlockIdRange blockIdRange = container.GetBlockIdRange();
		LevelInfo level = { blockIdRange.min, blockIdRange.size, 1 };
		levels.push_back(level);

		const int totalVoxelCount = MaxElement( level.size ) * blockResolution;
		while( totalVoxelCount >= level.voxelSize ) {			
			level.voxelSize <<= 1;
			
			level.min = VectorFloor(level.min.Cast<double>() / 2.0).Cast<int>();
			// level.size > 0 (otherwise totalVoxelCount = 0..)
			Vector3i levelMax = VectorCeil( (level.min + level.size).Cast<double>() / 2.0 ).Cast<int>();

			level.size = levelMax - level.min;
			levels.push_back(level);
		}
	}

	bool HasBlock( int level, const Vector3i &position ) {
		return container.ContainsBlock( "Density", position, level );
	}

	void GetBlock( int level, const Vector3i &position, const MutableArrayRef<uint16> &buffer ) {
		container.GetBlock( "Density", position, level, buffer );
	}

	void AddBlock( int level, const Vector3i &position, const ArrayRef<uint16> &buffer ) {
		container.AddBlock( "Density", position, level, buffer );
	}

	uint16 GetVoxel( const ArrayRef<uint16> &buffer, const Vector3i &position ) {
		return buffer[ layout( position + Vector3i::Constant( borderSize ) ) ];
	}

	uint16 & Voxel( const MutableArrayRef<uint16> &buffer, const Vector3i &position ) {
		return buffer.GetData()[ layout( position + Vector3i::Constant( borderSize ) ) ];
	}

	void SplitCoordinates( const Vector3i &position, Vector3i &blockIndex, Vector3i &localPosition ) {
		blockIndex = VectorFloor( position.Cast<double>() / blockResolution ).Cast<int>();
		localPosition = position - blockIndex * blockResolution;
	}

	Vector3i GetPositionInLevel( const Vector3i &position, int level ) {
		return VectorFloor( position.Cast<double>() / levels[level].voxelSize ).Cast<int>();
	}

	void GetCubeForMippedVoxel( int level, const Vector3i &position, Vector3i &min, Vector3i &max ) {
		int voxelSize = levels[level].voxelSize;
		min = position * voxelSize;
		max = min + Vector3i::Constant( voxelSize );
	}
};

/*Vector3i indexToCubeCorner(int i ) {
	return Vector3i( (i & 1) ? 1 : 0, (i & 2) ? 1 : 0, (i & 4) ? 1 : 0 );
}*/

const Vector3i indexToCubeCorner[] = {
	Vector3i( 0,0,0 ), Vector3i( 1,0,0 ), Vector3i( 0,1,0 ), Vector3i( 0,0,1 ),
	Vector3i( 1,1,0 ), Vector3i( 0,1,1 ), Vector3i( 1,0,1 ), Vector3i( 1,1,1 )
};

void generateMipmaps( VolumeCoordinates &volume ) {
	// to generate level:
	//	 access level - 1
	//	xRange_level_size = (xRange_{level-1}_size+1) / 2
	//	yRange_level_size and zRange_level_size likewise
	//	for x = xRange_{level-1}_min to xRange_{level-1}_max step 2
	//		for y = yRange_{level-1}_min to yRange_{level-1}_max step 2
	//			for z = zRange_{level-1}_min to zRange_{level-1}_max step 2
	//				grab all 8 blocks
	//				if all empty
	//					dont create a block
	//				otherwise
	//					iterate over all voxels and merge groups of 8
	//					add new block
		
	for( int level = 1 ; level < volume.levels.size() ; level++ ) {
		Iterator3D targetIterator( volume.levels[level].min, volume.levels[level].size );
		for( ; targetIterator != targetIterator.GetEndIterator() ; targetIterator++ ) {
			// only create new mipmap levels
			if( volume.HasBlock( level, targetIterator.ToVector() ) ) {
				continue;
			}

			Vector3i srcBlockOffset = targetIterator.ToVector() * 2;

			bool emptyBlock = true;
			for( int i = 0 ; i < 8 ; i++ ) {
				if( volume.HasBlock( level - 1, srcBlockOffset + indexToCubeCorner[ i ] ) ) {
					emptyBlock = false;
					break;
				}
			}
			if( emptyBlock ) {
				continue;
			}
			// TODO: deal with borders!
			// TODO: store the final mipmaps in one block?
			std::vector<uint16> blockData( volume.blockVoxelCount );
			std::vector<uint16> srcBlockData( volume.blockVoxelCount );
			for( int i = 0 ; i < 8 ; i++ ) {
				const Vector3i srcBlock = srcBlockOffset + indexToCubeCorner[ i ];

				if( !volume.HasBlock( level - 1, srcBlock ) ) {
					continue;
				}				
				volume.GetBlock( level - 1, srcBlock, srcBlockData );

				const Vector3i targetVoxelOffset = indexToCubeCorner[ i ] * (volume.blockResolution / 2);

				Iterator3D voxelIterator( Vector3i::Constant( 0 ), Vector3i::Constant( volume.blockResolution / 2 ) );
				for( ; voxelIterator != voxelIterator.GetEndIterator() ; voxelIterator++ ) {
					const Vector3i srcOffset = voxelIterator.ToVector() * 2;					

					uint16 value = 0;
					for( int voxelIndex = 0 ; voxelIndex < 8 ; voxelIndex++ ) {
						value = std::max( value, volume.GetVoxel( srcBlockData, srcOffset + indexToCubeCorner[ voxelIndex ] ) );
					}

					const Vector3i targetVoxelPosition = targetVoxelOffset + voxelIterator.ToVector();
					volume.Voxel( blockData, targetVoxelPosition ) = value;
				}
			}

			volume.AddBlock( level, targetIterator.ToVector(), blockData );
			std::cout << targetIterator.ToVector().X() << " " << targetIterator.ToVector().Y() << " " << targetIterator.ToVector().Z() << "\n";
		}
	}
}

struct DenseCache {
	VolumeCoordinates &volume;

	struct CacheEntry {
		bool cached;
		std::vector<uint16> data;

		CacheEntry() : cached( false ) {}
	};

	std::vector< CacheEntry > cacheEntries;
	std::vector< CacheEntry* > levelCache;

	DenseCache( VolumeCoordinates &volume ) : volume( volume ) {}

	void Init() {
		levelCache.resize( volume.levels.size() );

		int numBlocks = 0;
		for( int i = 0 ; i < volume.levels.size() ; i++ ) {
			const VolumeCoordinates::LevelInfo &levelInfo = volume.levels[i];
			numBlocks += levelInfo.size.X() * levelInfo.size.Y() * levelInfo.size.Z();
		}
		cacheEntries.resize( numBlocks );

		int blockIndex = 0;
		for( int i = 0 ; i < volume.levels.size() ; i++ ) {
			levelCache[i] = &cacheEntries.front() + blockIndex;

			const VolumeCoordinates::LevelInfo &levelInfo = volume.levels[i];
			blockIndex += levelInfo.size.X() * levelInfo.size.Y() * levelInfo.size.Z();
		}
	}

	const std::vector<uint16> & GetBlock( int level, const Vector3i &position ) {
		const VolumeCoordinates::LevelInfo &levelInfo = volume.levels[level];
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
/*
float distanceAABoxPoint( const Vector3f &min, const Vector3f &max, const Vector3f &point ) {
	Vector3f distances;
	for( int i = 0 ; i < 3 ; i++ ) {
		if( point[i] < min[i] ){ V
			distances[i] = min[i]-point[i];
		}
		else if( point[i] > max[i] ) {
			distances[i] = point[i]-max[i]; 
		}
		else {
			distances[i] = 0.f;
		}
	}
	return Length( distances );
}*/

/*
float distanceAABoxPoint( const Vector3f &min, const Vector3f &size, const Vector3f &point ) {
	Vector3f distances = point - min;
	for( int i = 0 ; i < 3 ; i++ ) {
		if( point[i] > size[i] ) {
			distances[i] -= size[i];
		}
		else if( point[i] > 0 ) {
			distances[i] = 0.f;
		}
	}
	return Length( distances );
}
*/

int squaredDistanceAABoxPoint( const Vector3i &min, const Vector3i &max, const Vector3i &point ) {
	Vector3i distance;
	for( int i = 0 ; i < 3 ; i++ ) {
		if( point[i] > max[i] ) {
			distance[i] = point[i] - max[i];
		}
		else if( point[i] > min[i] ) {
			distance[i] = 0.f;
		}
		else {
			distance[i] = min[i] - point[i];
		}
	}
	return LengthSquared( distance );
}

int squaredMaxDistanceAABoxPoint( const Vector3i &min, const Vector3i &max, const Vector3i &point ) {
	Vector3i distanceA = VectorAbs( point - min );
	Vector3i distanceB = VectorAbs( point - max );
	Vector3i distance = VectorMax( distanceA, distanceB );

	return LengthSquared( distance );
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

		if( minDistanceSquared < nearestMaxDistanceSquared ) {
			nearestCells.push_back( voxelPosition );
		}
	}
}

int findDistanceToNearestVoxel( DenseCache &cache, const Vector3i &refPosition, int minLevel = 0 ) {
	std::vector<Vector3i> currentLevel, candidates;

	int level = cache.volume.levels.size() - 1;
	// handle the upper-most level (1x1)
	{		
		Vector3i voxelPosition = cache.volume.GetPositionInLevel( refPosition, level );
		if( cache.GetVoxel( level, voxelPosition ) > 0 ) {
			currentLevel.push_back( voxelPosition );
		}
	}
	while( !currentLevel.empty() && level > minLevel ) {
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

		// only continue with the best candidates
		filterCandidates( cache, level, refPosition, candidates, currentLevel );
	}

	if( level == minLevel ) {

		int minDistanceSquared = squaredDistanceAABoxPoint( currentLevel[0], currentLevel[0] + Vector3i::Constant(1), refPosition );
		for( int i = 1 ; i < currentLevel.size() ; i++ ) {
			int distanceSquared = squaredDistanceAABoxPoint( currentLevel[i], currentLevel[i] + Vector3i::Constant(1), refPosition );
			minDistanceSquared = std::min( minDistanceSquared, distanceSquared );
		}
		return minDistanceSquared;
	}
	return INT_MAX;
}

int main(int argc, char* argv[]) 
{
	CoreLifeTimeHelper clth;

	try {
		Volume::FileBlockStorage shardFile; 
		if( !shardFile.Open( "P:\\BlenderScenes\\two_boxes_4.nvf", false ) ) {
			Log::Error( "shard2", "couldnt open the volume file!" );
		}

		VolumeCoordinates volume( shardFile );
		volume.Init();
		generateMipmaps( volume );

		Vector3i center(384,256,256);

		DenseCache cache( volume );
		cache.Init();

		std::cout << findDistanceToNearestVoxel( cache, center, 0 );

		shardFile.Flush();

	} catch (Exception& e) {
		std::cerr << e.what() << std::endl;
		std::cerr << e.GetDetailMessage() << std::endl;
		std::cerr << e.where() << std::endl;
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
