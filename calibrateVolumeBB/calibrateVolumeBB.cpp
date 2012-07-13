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

#include <niven.Core.IO.File.h>
#include <niven.Core.IO.FileSystem.h>
#include "niven.Engine.Geometry.GeometryStreamReader.h"
#include <niven.Engine.Geometry.GeometryStreamSplitter.h>
#include <iostream>

#include "niven.Core.VariantBuilder.h"
#include "niven.Core.Text.JsonWriter.h"

using namespace niven;

int main(int argc, char* argv[]) 
{
	CoreLifeTimeHelper clth;

	try {
		if( argc != 2 ) {
			std::cout << " [nvf] [ngs]\n";
			return -1;
		}
		std::cout << "Reading from " << argv[1] << " and " << argv[2] << ".\n";

		Volume::FileBlockStorage shardFile; 
		if( !shardFile.Open( /*"P:\\BlenderScenes\\two_boxes_4.nvf"*/ argv[1], false ) ) {
			Log::Error( "VolumeCalibration", "couldn't open the volume file!" );
		}

		Geometry::GeometryStreamReader reader;
		reader.ReadFrom( IO::FileSystem::OpenFile(/*"P:\\BlenderScenes\\two_boxes.ngs"*/ argv[2] ) );

		// calculate bbox for the whole geometry
		auto it = reader.GetIterator();
		if( !it.HasChunk() ) {
			return 0;
		}

		AxisAlignedBoundingBox3 bbox = it.GetChunkInfo().GetBounds();
		while( true ) {
			it.SkipChunk();

			if( !it.HasChunk() ) {
				break;
			}

			bbox.Merge( it.GetChunkInfo().GetBounds() );
		}

		Geometry::GeometryStreamSplitter::SplitSettings settings;
		settings.blockResolution = 256;
		settings.border = 0;
		settings.paddingFactor = 0;
		// TODO: fix: hardcoded subdivision count...
		settings.subdivisions = 4; // >_>

		Geometry::GeometryStreamSplitter::Grid grid = Geometry::GeometryStreamSplitter::CreateGrid( bbox, settings );
		Vector3f offset = grid.worldBounds.GetMinimum();
		float blockSize = grid.blockSize;

		shardFile.SetAttribute( "offset", ArrayRef<Vector3f>( offset ) );
		shardFile.SetAttribute( "blockSize", ArrayRef<float>( blockSize ) );

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