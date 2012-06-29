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

typedef std::vector<byte> ByteVector;

// only one blockSize and borderSize
struct DensityShardFile {
	MemoryLayout3D layout;
	Volume::FileBlockStorage container;
	int blockSize;
	int borderSize;
	int totalSize;
	int blockVoxelCount;

	bool Open( const IO::Path &path, const bool readOnly = false ) {
		if( container.Open( path, readOnly ) ) {
			auto layerDescriptor = container.GetLayerDescriptor( "Density" );

			blockSize = layerDescriptor.blockResolution;
			borderSize = layerDescriptor.borderSize;
			totalSize = blockSize + borderSize*2;
			blockVoxelCount = totalSize * totalSize * totalSize;

			layout = MemoryLayout3D( totalSize );

			return true;
		}
		return false;
	}

	bool ContainsBlock( const Vector3i &xyz ) {
		return container.ContainsBlock( "Density", xyz, 0 );
	}

	std::vector<uint16> GetBlock( const Vector3i &xyz ) {
		std::vector<uint16> block( blockVoxelCount );

		container.GetBlock( "Density", xyz, 0, MutableArrayRef<uint16>( &block.front(), blockVoxelCount ) );

		return block;
	}

	uint16 GetVoxel( const ArrayRef<uint16> &block, const Vector3i &xyz ) {
		return block[ layout( xyz ) ];
	}

	Vector3i getBlockXYZ( const Vector3i &xyz ) {
		return xyz / blockSize;
	}

	void decomposeIndex( const Vector3i &xyz, Vector3i &blockXYZ, Vector3i &intraBlockXYZ ) {
		blockXYZ = getBlockXYZ( xyz );
		intraBlockXYZ = xyz - blockXYZ * blockSize;
	}

	Vector3i composeIndex( const Vector3i &blockXYZ, const Vector3i &intraBlockXYZ ) {
		return blockXYZ*blockSize + intraBlockXYZ;
	}

};

struct Cache {
	typedef std::pair<Vector3i, std::vector<uint16>> CacheEntry;
	std::vector<CacheEntry> blocks;
	int lastUsed;

	enum CacheType {
		UNIQUE_X = 1,
		UNIQUE_Y = 2,
		UNIQUE_Z = 4
	};
	unsigned cacheType;

	static unsigned CacheTypeFromAxisOrientedNormal(const Vector3i &direction) {
		if( direction.X() != 0 ) {
			return UNIQUE_Y | UNIQUE_Z;
		}
		if( direction.Y() != 0 ) {
			return UNIQUE_X | UNIQUE_Z;
		}
		if( direction.Z() != 0 ) {
			return UNIQUE_X | UNIQUE_Y;
		}
		return UNIQUE_X | UNIQUE_Y | UNIQUE_Z;
	}

	DensityShardFile &shardFile;

	Cache( DensityShardFile &shardFile, unsigned cacheType ) 
		: shardFile( shardFile ), cacheType( cacheType ), lastUsed( -1 ) {}
	
	ArrayRef<uint16> GetBlock( const Vector3i &blockXYZ ) {
		if( lastUsed != -1 && blocks[lastUsed].first == blockXYZ ) {
			return ArrayRef<uint16>( (uint16*) &blocks[lastUsed].second.front(), shardFile.blockVoxelCount );
		}

		// search for the block in the cache
		auto entry = std::find_if( blocks.begin(), blocks.end(), [&] (const CacheEntry &entry) -> bool { 
			if( (cacheType & UNIQUE_X) && blockXYZ.X() != entry.first.X() ) {
				return false;
			}
			if( (cacheType & UNIQUE_Y) && blockXYZ.Y() != entry.first.Y() ) {
				return false;
			}
			if( (cacheType & UNIQUE_Z) && blockXYZ.Z() != entry.first.Z() ) {
				return false;
			}
			return true;
		});

		if( entry != blocks.end() && entry->first == blockXYZ ) {
			lastUsed = &*entry - &blocks.front();
			return ArrayRef<uint16>( (uint16*) &entry->second.front(), shardFile.blockVoxelCount );
		}

		std::vector<uint16> blockData;
		if( shardFile.ContainsBlock( blockXYZ ) ) {
			blockData = shardFile.GetBlock( blockXYZ );
		}

		if( entry != blocks.end() ) {
			lastUsed = &*entry - &blocks.front();
			entry->first = blockXYZ;
			entry->second = blockData;			
			return ArrayRef<uint16>( (uint16*) &entry->second.front(), !blockData.empty() ? shardFile.blockVoxelCount : 0 );
		}
		else {
			lastUsed = blocks.size();
			blocks.push_back( std::make_pair( blockXYZ, blockData ) );
			return ArrayRef<uint16>( (uint16*) &blocks.back().second.front(), !blockData.empty() ? shardFile.blockVoxelCount : 0 );
		}
	}

	uint16 GetVoxel( const Vector3i &xyz ) {
		Vector3i blockXYZ, intraBlockXYZ;
		shardFile.decomposeIndex( xyz, blockXYZ, intraBlockXYZ );
		ArrayRef<uint16> blockData = GetBlock( blockXYZ );
		if( blockData.empty() ) {
			return 0;
		}
		return shardFile.GetVoxel( blockData, intraBlockXYZ + Vector3i::Constant( shardFile.borderSize ) );
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
