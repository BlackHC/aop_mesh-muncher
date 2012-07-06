#include "Core/inc/Core.h"
#include "Core/inc/Exception.h"
#include "Core/inc/Log.h"
#include "Core/inc/nString.h"

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
#include "findDistances.h"

#include "gtest.h"

using namespace niven;

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
		Vector3i blockPosition = position - blockIndex * densityLayer.blockResolution;

		int blockSize = densityLayer.blockResolution + densityLayer.borderSize*2;
		std::vector<uint16> block( blockSize * blockSize * blockSize );

		MemoryLayout3D layout( blockSize );
		if( volume.ContainsBlock( "Density", blockIndex, 0 ) ) {
			volume.GetBlock( "Density", blockIndex, 0, block );

			block[ layout( blockPosition ) ] = density;

			volume.UpdateBlock( "Density", blockIndex, 0, block );
		}
		else {
			block[ layout( blockPosition ) ] = density;
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
	EXPECT_EQ( INT_MAX, findSquaredDistanceToNearestVoxel( cache, Vector3i( 0,0,0 ), 0 ) );
	EXPECT_EQ( INT_MAX, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
}

TEST_F( DistanceTests, singleVoxel ) {
	using namespace niven::Volume;

	MemoryBlockStorage blockStorage;
	setupVolume( blockStorage, 16, 0 );
	setDensity( blockStorage, Vector3i(0,0,0), 65535 );

	MipVolume volume( blockStorage );
	DenseCache cache( volume );
	EXPECT_EQ( 0, findSquaredDistanceToNearestVoxel( cache, Vector3i( 0,0,0 ), 0 ) );
	EXPECT_EQ( 254*254, findSquaredDistanceToNearestVoxel( cache, Vector3i( 255,0,0 ), 0 ) );
	EXPECT_EQ( 0, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
	EXPECT_EQ( 254*254, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 255,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
	EXPECT_EQ( INT_MAX, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 255,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_NO_MATCH; } ) );
}

TEST_F( DistanceTests, twoVoxels ) {
	using namespace niven::Volume;

	MemoryBlockStorage blockStorage;
	setupVolume( blockStorage, 16, 0 );
	setDensity( blockStorage, Vector3i(0,0,0), 65535 );
	setDensity( blockStorage, Vector3i(15,0,0), 65535 );

	MipVolume volume( blockStorage );
	DenseCache cache( volume );
	EXPECT_EQ( 0, findSquaredDistanceToNearestVoxel( cache, Vector3i( 0,0,0 ), 0 ) );
	EXPECT_EQ( 14*14, findSquaredDistanceToNearestVoxel( cache, Vector3i( 0,15,0 ), 0 ) );
	EXPECT_EQ( 0, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
	EXPECT_EQ( 15*15, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) -> ConditionedVoxelType { if( min.X() > 0 ) return CVT_MATCH; else if( max.X() > 1 ) return CVT_PARTIAL; else return CVT_NO_MATCH; } ) );
	EXPECT_EQ( 3*3, findSquaredDistanceToNearestVoxel( cache, Vector3i( 0,4,0 ), 0 ) );

	// left and right distances different because 0|0|0 is at the corner of voxel 0|0|0!
}

TEST_F( DistanceTests, multipleBlocks ) {
	using namespace niven::Volume;

	MemoryBlockStorage blockStorage;
	setupVolume( blockStorage, 4, 0 );
	for( int i = 0 ; i < 8 ; i++ ) {
		setDensity( blockStorage, 255 * indexToCubeCorner[i], 65535 );
	}

	MipVolume volume( blockStorage );
	DenseCache cache( volume );
	EXPECT_EQ( 0, findSquaredDistanceToNearestVoxel( cache, Vector3i( 0,0,0 ), 0 ) );
	EXPECT_EQ( 14*14, findSquaredDistanceToNearestVoxel( cache, Vector3i( 15,0,0 ), 0 ) );
	EXPECT_EQ( 0, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
	EXPECT_EQ( 255*255, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( -255,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_MATCH; } ) );
	EXPECT_EQ( INT_MAX, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 255,0,0 ), []( const Vector3i &min, const Vector3i &max ) { return CVT_NO_MATCH; } ) );
	EXPECT_EQ( 255*255, findSquaredDistanceToNearestConditionedVoxel( cache, Vector3i( 0,0,0 ), []( const Vector3i &min, const Vector3i &max ) -> ConditionedVoxelType { if( min.X() > 0 ) return CVT_MATCH; else if( max.X() > 1 ) return CVT_PARTIAL; else return CVT_NO_MATCH; } ) );
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
