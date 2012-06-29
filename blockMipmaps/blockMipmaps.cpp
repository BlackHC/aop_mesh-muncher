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
#include <iostream>
#include "niven.Core.MemoryLayout.h"
#include "niven.Core.Iterator3D.h"

using namespace niven;

// assuming even blockResolution

struct MipVolume {
	Volume::IBlockStorage &container;
	
	int borderSize;
	Vector3i min, size;

	struct LevelInfo {
		int voxelSize;
		int blockResolution;
		int voxelCount;

		MemoryLayout3D layout;
	};

	std::vector<LevelInfo> levels;

	MipVolume( Volume::IBlockStorage &container ) : container( container ) {}

	void Init() {
		auto layerDescriptor = container.GetLayerDescriptor( "Density" );

		int blockResolution = layerDescriptor.blockResolution;
		borderSize = layerDescriptor.borderSize;

		Volume::IBlockStorage::BlockIdRange blockIdRange = container.GetBlockIdRange();
		min = blockIdRange.min;
		size = blockIdRange.size;
		
		LevelInfo level;
		level.voxelSize = 1;
		level.blockResolution = blockResolution;
		int totalSize = blockResolution + borderSize*2;
		level.voxelCount = totalSize * totalSize * totalSize;
		level.layout = MemoryLayout3D( totalSize );
		levels.push_back(level);

		while( level.blockResolution > 1 ) {			
			level.voxelSize <<= 1;
			level.blockResolution >>= 1;
			
			totalSize = level.blockResolution + borderSize*2;
			level.voxelCount = totalSize * totalSize * totalSize;
			level.layout = MemoryLayout3D( totalSize );

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

	uint16 GetVoxel( const ArrayRef<uint16> &blockData, int level, const Vector3i &position ) {
		return blockData[ levels[level].layout( position + Vector3i::Constant( borderSize ) ) ];
	}

	uint16 & Voxel( const MutableArrayRef<uint16> &blockData, int level, const Vector3i &position ) {
		return blockData[ levels[level].layout( position + Vector3i::Constant( borderSize ) ) ];
	}

	void SplitCoordinates( int level, const Vector3i &position, Vector3i &blockIndex, Vector3i &localPosition ) {
		int blockResolution = levels[level].blockResolution;
		blockIndex = VectorFloor( position.Cast<double>() / blockResolution).Cast<int>();
		localPosition = position - blockIndex * blockResolution;
	}

	Vector3i GetPositionInLevel( const Vector3i &position, int level ) {
		return VectorFloor( position.Cast<double>() / levels[level].voxelSize ).Cast<int>();
	}
};

Vector3i indexToCubeCorner(int i ) {
	return Vector3i( (i & 1) ? 1 : 0, (i & 2) ? 1 : 0, (i & 4) ? 1 : 0 );
}

void generateMipmaps( MipVolume &volume ) {
	for( int level = 1 ; level < volume.levels.size() ; level++ ) {
		Iterator3D targetIterator( volume.min, volume.size );
		for( ; targetIterator != targetIterator.GetEndIterator() ; targetIterator++ ) {
			Vector3i blockPosition = targetIterator.ToVector();

			if( volume.HasBlock( level - 1, blockPosition ) ) {
				continue;
			}

			// TODO: deal with borders!
			// TODO: store the final mipmaps in one block?
			std::vector<uint16> blockData( volume.levels[level].voxelCount );
			std::vector<uint16> srcBlockData( volume.levels[level-1].voxelCount );

			volume.GetBlock( level - 1, blockPosition, srcBlockData );

			Iterator3D voxelIterator( Vector3i::Constant( 0 ), Vector3i( volume.levels[level].blockResolution );
			for( ; voxelIterator != voxelIterator.GetEndIterator() ; voxelIterator++ ) {
				const Vector3i srcOffset = voxelIterator.ToVector() * 2;					

				uint16 value = 0;
				for( int voxelIndex = 0 ; voxelIndex < 8 ; voxelIndex++ ) {
					value = std::max( value, volume.GetVoxel( srcBlockData, level - 1, srcOffset + indexToCubeCorner( voxelIndex ) ) );
				}

				volume.Voxel( blockData, level, voxelIterator.ToVector() ) = value;
			}

			volume.AddBlock( level, targetIterator.ToVector(), blockData );
		}
	}
}

struct DenseCache {
	MipVolume &volume;

	struct CacheEntry {
		bool cached;
		std::vector<uint16> data;

		CacheEntry() : cached( false ) {}
	};

	std::vector< CacheEntry > cacheEntries;
	std::vector< CacheEntry* > levelCache;

	void Init() {
		int numBlocksPerLevel = volume.size.X() * volume.size.Y() * volume.size.Z();
		int numBlocks = numBlocksPerLevel * volume.levels.size();
		cacheEntries.resize( numBlocks );

		levelCache.resize( volume.levels.size() );
		for( int i = 0 ; i < volume.levels.size() ; i++ ) {
			levelCache[i] = &cacheEntries.front() + numBlocksPerLevel * i;
		}
	}

	const std::vector<uint16> & GetBlock( int level, const Vector3i &position ) {
		const MipVolume::LevelInfo &levelInfo = volume.levels[level];
		int index = position.X() + position.Y() * volume.size.X() + position.Z() * (volume.size.X() * volume.size.Y());
		CacheEntry &cacheEntry = levelCache[level][index];
		if( cacheEntry.cached ) {
			return cacheEntry.data;
		}

		cacheEntry.cached = true;
		if( volume.HasBlock( level, position ) ) {
			cacheEntry.data.resize( levelInfo.voxelCount );
			volume.GetBlock( level, position, cacheEntry.data );
		}
	}

	uint16 GetVoxel( int level, const Vector3i &position ) {
		Vector3i blockIndex, localPosition;
		volume.SplitCoordinates( level, volume.GetPositionInLevel( position, level ), blockIndex, localPosition );

		const std::vector<uint16> &blockData = GetBlock( level, blockIndex );
		if( blockData.empty() ) {
			return 0;
		}
		return volume.GetVoxel( blockData, level, localPosition );
	}
};

bool isEmpty( Cache &cache, const Vector3i &minXYZ, const Vector3i &maxXYZ ) {
	for( Iterator3D iter = Iterator3D::FromMinMax(minXYZ, maxXYZ) ; iter != iter.GetEndIterator() ; iter++ ) {
		if( cache.GetVoxel( iter.ToVector() ) != 0 ) {
			return false;
		}
	}
	return true;
}

int findMaxGapAlongAxis( DensityShardFile &shardFile, const Vector3i &minXYZ, const Vector3i &maxXYZ, const Vector3i &direction, int maxDistance ) {
	Cache cache( shardFile, Cache::CacheTypeFromAxisOrientedNormal( direction ) );

	for( int i = 0 ; i < maxDistance ; i++ ) {
		if( !isEmpty( cache, minXYZ + i*direction, maxXYZ + i*direction ) ) {
			return i;
		}
	}
	return maxDistance;
}

bool findDistance( Cache &cache, int x, int y, int minZ, int maxZ, const Vector2i &refXY, double &minDistance ) {
	bool foundNonEmpty = false;

	for( int cx = 0 ; cx < cache.shardFile.blockSize ; cx++ ) {
		for( int cy = 0 ; cy < cache.shardFile.blockSize ; cy++ ) {
			Vector2i XY( cx + cache.shardFile.blockSize * x, cy + cache.shardFile.blockSize * y );
			Vector3i minXYZ( XY, minZ );
			Vector3i maxXYZ( XY, maxZ );

			if( !isEmpty( cache, minXYZ.ZXY(), maxXYZ.ZXY() ) ) {				
				double distance = Length( (XY - refXY).Cast<double>() );
				if( distance < minDistance ) {
					foundNonEmpty = true;
					minDistance = distance;
				}
			}
		}
	}

	return foundNonEmpty;
}

double findMaxGap( DensityShardFile &shardFile, const Vector3i &minXYZ, int zHeight, int maxDistance ) {
	Cache cache( shardFile, Cache::UNIQUE_Z | Cache::UNIQUE_X | Cache::UNIQUE_Y );
	// best traversal order:
	// 2 1 2  
	// 1 0 1
	// 2 1 2
	//
	// implemented traversal order:
	// 1 1 1
	// 1 0 1
	// 1 1 1
	const Vector2i refXY = minXYZ.XY();
	double minDistance = maxDistance;
	for( int i = 0 ; i < (maxDistance + shardFile.blockSize - 1) / shardFile.blockSize ; i++ ) {
		// 1 1 2
		// 4 0 2
		// 3 3 2
		bool foundNonEmpty = false;
		for( int x = -i ; x < i ; x++ ) {
			int y = i;
			foundNonEmpty = foundNonEmpty || findDistance( cache, x, y, minXYZ.Z(), minXYZ.Z() + zHeight, refXY, minDistance );
		}
		for( int y = -i ; y <= i ; y++ ) {
			int x = i;
			foundNonEmpty = foundNonEmpty || findDistance( cache, x, y, minXYZ.Z(), minXYZ.Z() + zHeight, refXY, minDistance );
		}
		for( int x = -i ; x < i ; x++ ) {
			int y = -i;
			foundNonEmpty = foundNonEmpty || findDistance( cache, x, y, minXYZ.Z(), minXYZ.Z() + zHeight, refXY, minDistance );
		}
		for( int y = -i + 1 ; y < i ; y++ ) {
			int x = -i;
			foundNonEmpty = foundNonEmpty || findDistance( cache, x, y, minXYZ.Z(), minXYZ.Z() + zHeight, refXY, minDistance );
		}

		if( foundNonEmpty ) {
			break;
		}
	}

	return minDistance;
}

int main(int argc, char* argv[]) 
{
	CoreLifeTimeHelper clth;

	try {
		DensityShardFile shardFile; 
		if( !shardFile.Open( "P:\\BlenderScenes\\two_boxes_4.nvf", true ) ) {
			Log::Error( "shard2", "couldnt open the volume file!" );
		}

		Vector3i center(384,256,256);
		{
			Vector3i minXYZ = center - Vector3i( 32, 32, 0 );
			Vector3i maxXYZ = center + Vector3i( 32, 32, 0 );
			int maxDistance = findMaxGapAlongAxis( shardFile, minXYZ, maxXYZ, Vector3i::CreateUnit(2), 2048 );
			std::cout << "maxDistance: " << maxDistance << std::endl;
		}

		{
			double maxGap = findMaxGap( shardFile, center + Vector3i( 0,0, -32 ), 64, 2048 );
			std::cout << "maxGap: " << maxGap << std::endl;
		}
	} catch (Exception& e) {
		std::cerr << e.what() << std::endl;
		std::cerr << e.where() << std::endl;
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
