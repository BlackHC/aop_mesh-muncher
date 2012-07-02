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

#include "mipVolume.h"
#include "cache.h"

#include "memoryBlockStorage.h"

#include "gtest.h"

using namespace niven;

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

int findDistanceToNearestVoxel( DenseCache &cache, const Vector3i &refPosition, int minLevel = 0 ) {
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
		int minDistanceSquared = squaredDistanceAABoxPoint( candidates[0], candidates[0] + Vector3i::Constant(1), refPosition );
		for( int i = 1 ; i < candidates.size() ; i++ ) {
			int distanceSquared = squaredDistanceAABoxPoint( candidates[i], candidates[i] + Vector3i::Constant(1), refPosition );
			minDistanceSquared = std::min( minDistanceSquared, distanceSquared );
		}
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

enum ConditionedVoxelType {
	CVT_NO_MATCH = 0,
	CVT_PARTIAL = 1,
	CVT_MATCH = 2
};

ConditionedVoxelType operator &&( const ConditionedVoxelType a, const ConditionedVoxelType b ) {
	return ConditionedVoxelType( std::min( a, b ) );
}

ConditionedVoxelType operator ||( const ConditionedVoxelType a, const ConditionedVoxelType b ) {
	return ConditionedVoxelType( std::max( a, b ) );
}

ConditionedVoxelType operator !(const ConditionedVoxelType a) {
	return ConditionedVoxelType( CVT_MATCH - a );
}

int findDistanceToNearestConditionedVoxel( DenseCache &cache, const Vector3i &refPosition, const std::function<ConditionedVoxelType (const Vector3i &min, const Vector3i &max)> &conditioner, int minLevel = 0 ) {
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
			for( int i = 0 ; i < candidates.size() ; i++ ) {
				int distanceSquared = squaredDistanceAABoxPoint( candidates[i], candidates[i] + Vector3i::Constant(1), refPosition );
				minDistanceSquared = std::min( minDistanceSquared, distanceSquared );
			}
			for( int i = 0 ; i < partialCandidates.size() ; i++ ) {
				int distanceSquared = squaredDistanceAABoxPoint( partialCandidates[i], partialCandidates[i] + Vector3i::Constant(1), refPosition );
				minDistanceSquared = std::min( minDistanceSquared, distanceSquared );
			}
			return minDistanceSquared;
		}
	}

	return INT_MAX;
}

struct DistanceTests : public ::testing::Test {
	CoreLifeTimeHelper helper;
	
	static void setupVolume( niven::Volume::IBlockStorage &volume, int blockSize, int borderSize ) {
		using namespace niven::Volume;

		IBlockStorage::Metadata metadata;
		metadata.blockSize = blockSize;
		volume.SetMetadata( metadata );

		IBlockStorage::LayerDescriptor densityLayer;
		densityLayer.blockResolution = blockSize;
		densityLayer.borderSize = borderSize;
		densityLayer.dataType = DataType::UInt16;
		
		volume.AddLayer( "Density", densityLayer );
	}

	static void setDensity( niven::Volume::IBlockStorage &volume, niven::Vector3i position, const uint16 density ) {
		using namespace niven::Volume;

		IBlockStorage::LayerDescriptor densityLayer = volume.GetLayerDescriptor( "Density" );
		Vector3i blockIndex = position / densityLayer.blockResolution;

		int blockSize = densityLayer.blockResolution + densityLayer.borderSize*2;
		std::vector<uint16> block( blockSize * blockSize * blockSize );

		MemoryLayout3D layout( blockSize );
		if( volume.ContainsBlock( "Density", blockIndex, 0 ) ) {
			volume.GetBlock( "Density", blockIndex, 0, block );

			block[ layout( position ) ] = density;

			volume.UpdateBlock( "Density", blockIndex, 0, block );
		}
		else {
			block[ layout( position ) ] = density;
			volume.AddBlock( "Density", blockIndex, 0, block );
		}
	}
};

TEST_F( DistanceTests, emptyVolume ) {
	using namespace niven::Volume;

	MemoryBlockStorage blockStorage;
	setupVolume( blockStorage, 256, 0 );

	MipVolume volume( blockStorage );
	DenseCache cache( volume );
	EXPECT_EQ( INT_MAX, findDistanceToNearestVoxel( cache, Vector3i( 0,0,0 ), 0 ) );
	EXPECT_EQ( INT_MAX, findDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
}

TEST_F( DistanceTests, singleVoxel ) {
	using namespace niven::Volume;

	MemoryBlockStorage blockStorage;
	setupVolume( blockStorage, 16, 0 );
	setDensity( blockStorage, Vector3i(0,0,0), 65535 );

	MipVolume volume( blockStorage );
	DenseCache cache( volume );
	EXPECT_EQ( 0, findDistanceToNearestVoxel( cache, Vector3i( 0,0,0 ), 0 ) );
	EXPECT_EQ( 254*254, findDistanceToNearestVoxel( cache, Vector3i( 255,0,0 ), 0 ) );
	EXPECT_EQ( 0, findDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
	EXPECT_EQ( 254*254, findDistanceToNearestConditionedVoxel( cache, Vector3i( 255,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
	EXPECT_EQ( INT_MAX, findDistanceToNearestConditionedVoxel( cache, Vector3i( 255,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_NO_MATCH; } ) );
}

TEST_F( DistanceTests, twoVoxels ) {
	using namespace niven::Volume;

	MemoryBlockStorage blockStorage;
	setupVolume( blockStorage, 16, 0 );
	setDensity( blockStorage, Vector3i(0,0,0), 65535 );
	setDensity( blockStorage, Vector3i(15,0,0), 65535 );

	MipVolume volume( blockStorage );
	DenseCache cache( volume );
	EXPECT_EQ( 0, findDistanceToNearestVoxel( cache, Vector3i( 0,0,0 ), 0 ) );
	EXPECT_EQ( 14*14, findDistanceToNearestVoxel( cache, Vector3i( 0,15,0 ), 0 ) );
	EXPECT_EQ( 0, findDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
	EXPECT_EQ( 15*15, findDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) -> ConditionedVoxelType { if( min.X() > 0 ) return CVT_MATCH; else if( max.X() > 1 ) return CVT_PARTIAL; else return CVT_NO_MATCH; } ) );
	EXPECT_EQ( 3*3, findDistanceToNearestVoxel( cache, Vector3i( 0,4,0 ), 0 ) );

	// left and right distances different because 0|0|0 is at the corner of voxel 0|0|0!
}

/*
int main(int argc, char* argv[]) 
{
	CoreLifeTimeHelper clth;

	try {
		Volume::FileBlockStorage shardFile; 
		if( !shardFile.Open( "P:\\BlenderScenes\\two_boxes_4.nvf", false ) ) {
			Log::Error( "shard2", "couldnt open the volume file!" );
		}

		MipVolume volume( shardFile );
		volume.GenerateMipmaps();

		Vector3i center(384,256,256);
		int yMin = center.Z()-32;
		int yMax = center.Z()+32;

		DenseCache cache( volume );

		//std::cout << findDistanceToNearestVoxel( cache, center, 0 );
		std::cout << findDistanceToNearestConditionedVoxel( cache, center, [yMin,yMax](const Vector3i &min, const Vector3i &max) -> ConditionedVoxelType {
			if( max.Z() < yMin || yMax < min.Z() ) {
				return CVT_NO_MATCH;
			}
			if( yMin <= min.Z() && max.Z() <= yMax ) {
				return CVT_MATCH;
			}
			return CVT_PARTIAL;
		} );

		shardFile.Flush();

	} catch (Exception& e) {
		std::cerr << e.what() << std::endl;
		std::cerr << e.GetDetailMessage() << std::endl;
		std::cerr << e.where() << std::endl;
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}*/
