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
#include "niven.Volume.ShardFile.h"
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
	Volume::ShardFile container;
	int blockSize;
	int borderSize;
	int totalSize;

	bool Open( const IO::Path &path, const bool readOnly = false ) {
		if( container.Open( path, readOnly ) ) {
			auto layerDescriptor = container.GetLayerDescriptor( "Density" );

			blockSize = layerDescriptor.blockSize;
			borderSize = layerDescriptor.borderSize;
			totalSize = blockSize + borderSize*2;
			layout = MemoryLayout3D( totalSize );

			return true;
		}
		return false;
	}

	template<typename T>
	std::vector<T> GetBlock( const Vector3i &xyz ) {
		std::vector<T> block( totalSize * totalSize * totalSize );

		container.GetBlock( "Density", xyz, 0, MutableArrayRef( &block.front() ) );

		return block;
	}

	template<typename T>
	T & GetVoxel( ArrayRef<T> &block, const Vector3i &xyz ) {
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
/*
Vector3i getStartBlock( const Vector3i &xyz, int blockSize ) {
	return xyz / blockSize;
}

Vector3i getEndBlock( const Vector3i &xyz, int blockSize ) {
	return (xyz + Vector3i::Constant( blockSize - 1 ) ) / blockSize;
}

Vector3i getNumBlocks( const Vector3i &minXYZ, const Vector3i &maxXYZ, int blockSize ) {
	const Vector3i minBlockXYZ = getStartBlock( minXYZ, blockSize );
	const Vector3i maxBlockXYZ = getEndBlock( maxXYZ, blockSize );
	const Vector3i deltaBlockXYZ = maxBlockXYZ - minBlockXYZ;
	return deltaBlockXYZ.X() * deltaBlockXYZ.Y() * deltaBlockXYZ.Z();
}

// func( Vector3i xyz, Vector3i blockXYZ )
// cache( Vector3i blockXYZ )
template<typename F, typename G>
void blockFor( Vector3i minXYZ, Vector3i maxXYZ, int blockSize, F func, G cache ) {
	for( int bz = minXYZ.Z() ; bz < maxXYZ.Z() ; bz += blockSize ) {
		for( int by = minXYZ.Y() ; by < maxXYZ.Y() ; by += blockSize ) {
			for( int bx = minXYZ.X() ; bx < maxXYZ.X() ; bx += blockSize ) {
				Vector3i blockXYZ = Vector3i( bx, by, bz ) / blockSize;
				if( cache( blockXYZ ) ) {
					const int mx = std::min( maxXYZ.X() - bx, blockSize );
					const int my = std::min( maxXYZ.Y() - by, blockSize );
					const int mz = std::min( maxXYZ.Z() - bz, blockSize );
		
					for( int z = 0 ; z < mz ; z++ ) {
						for( int y = 0 ; y < my ; y++ ) {
							for( int x = 0 ; x < mx ; x++ ) {
								func( Vector3i( x,y,z ), blockXYZ );
							}
						}
					}
				}
			}
		}
	}		
}
*/

struct Cache {
	typedef std::pair<Vector3i, std::vector<byte>> CacheEntry;
	std::vector<CacheEntry> blocks;

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
		: shardFile( shardFile ), cacheType( cacheType ) {}
	
	template<typename T>
	const ArrayRef<T> & GetBlock( const Vector3i &blockXYZ ) {
		// search for the block in the cache
		auto entry = std::find_if( blocks.begin(), blocks.end(), [&] (const CacheEntry &entry) { 
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

		if( entry != blocks.end() && entry.first == blockXYZ ) {
			return (void*) &blocks.second.front();
		}

		std::vector<byte> blockData = shardFile.GetBlock<byte>( blockXYZ );
		if( entry != blocks.end() ) {
			entry->first = blockXYZ
			entry->second = blockData;
		}
		else {
			blocks.push_back( std::make_pair( blockXYZ, blockData ) );
		}

		return (void*) &blockData.front();
	}

	template<typename T>
	T & GetVoxel( const Vector3i &xyz ) {
		Vector3i blockXYZ, intraBlockXYZ;
		shardFile.decomposeIndex( xyz, blockXYZ, intraBlockXYZ );
		return shardFile.GetVoxel( GetBlock( blockXYZ ), intraBlockXYZ + Vector3i::Constant( shardFile.borderSize ) );
	}
};

bool isEmpty( Cache &cache, const Vector3i &minXYZ, const Vector3i &maxXYZ ) {
	for( Iterator3D iter = Iterator3D::FromMinMax(minXYZ, maxXYZ) ; iter != iter.GetEndIterator() ; iter++ ) {
		if( cache.GetVoxel<uint16>( iter.ToVector() ) != 0 ) {
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

			if( !isEmpty( cache, minXYZ, maxXYZ ) ) {				
				double distance = Length( XY - refXY );
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
	Cache cache( shardFile, Cache::UNIQUE_Z );
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
	double minDistance = maxDistance * shardFile.blockSize;
	for( int i = 0 ; i < maxDistance ; i++ ) {
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

/*
struct ShardVolume {
	MemoryLayout3D blockLayout;
	Volume::ShardFile shardFile;
	int blockSize;

	ShardVolume() : blockLayout( 256 + 2 ) {}

	uint16 getXYZ( const Vector3i &position ) {
		Vector3i blockPosition = position / blockSize;

		if( !shardFile.ContainsBlock( "Density", blockPosition, 0 ) ) {
			return 0;
		}

		Vector3i cellPosition = position - blockPosition * blockSize;
		MutableArrayRef<uint16> buffer;
		shardFile.GetBlock( "Density", position, 0, buffer );

		return buffer[ blockLayout( cellPosition ) ];
	}
};*/

int main(int argc, char* argv[]) 
{
	CoreLifeTimeHelper clth;

	try {
		Volume::ShardFile shardFile;
		if( !shardFile.Open( "P:\\BlenderScenes\\two_boxes_4.nsf") ) {
			Log::Error( "shard2", "couldnt open the shard file!" );
		}


	} catch (Exception& e) {
		std::cerr << e.what() << std::endl;
		std::cerr << e.where() << std::endl;
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
