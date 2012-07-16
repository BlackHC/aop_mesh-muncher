#include "Core/inc/Core.h"
#include "Core/inc/Exception.h"
#include "Core/inc/Log.h"
#include "Core/inc/nString.h"

#include "niven.Core.IO.Path.h"

#include "niven.Volume.FileBlockStorage.h"
#include "niven.Volume.MarchingCubes.h"
#include "niven.Volume.Volume.h"

#include "niven.Core.Geometry.Plane.h"

#include "niven.Core.MemoryLayout.h"
#include "niven.Core.Iterator3D.h"

#include "niven.Core.Math.VectorToString.h"

#include "niven.Engine.Sample.h"
#include "niven.Core.Math.3dUtility.h"
#include "niven.Engine.Spatial.AxisAlignedBoundingBox.h"
#include "niven.Core.Geometry.Ray.h"

#include <iostream>

#include "Eigen/Eigen"

using namespace niven;

/*
TODO: use different layer for mipmaps!!
*/

#include "volumePlacer.h"

int main(int argc, char* argv[]) 
{
	CoreLifeTimeHelper clth;

	try {
		Volume::FileBlockStorage shardFile; 
		if( !shardFile.Open( "P:\\BlenderScenes\\two_boxes_4.nvf", true ) ) {
			Log::Error( "shard2", "couldn't open the volume file!" );
		}

		MipVolume volume( shardFile );
		DenseCache cache( volume );

		UnorderedDistanceContext::setDirections();

		const Vector3i min(850,120,120);
		const Vector3i size(280, 280, 280);
		Probes probes(min, size, 16);

		sampleProbes( cache, probes );

		ProbeDatabase database;

		addObjectInstanceToDatabase( probes, database, probes.getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i(0,0,0), Vector3i(4,1,1) ) ), 0 );
		addObjectInstanceToDatabase( probes, database, probes.getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i(5,5,5), Vector3i(4,1,1) ) ), 1 );

		ProbeDatabase::WeightedCandidateIdVector result = findCandidates( probes, database, probes.getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i( 8, 8, 8 ), Vector3i( 8, 8, 8 ) ) ) );
		printCandidates( std::cout, result );

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