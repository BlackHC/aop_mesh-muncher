#include <niven.Core.Core.h>
#include <niven.Core.Exception.h>
#include <niven.Core.Log.h>
#include <niven.Core.String.h>

#include <niven.Core.IO.Path.h>

#include <niven.Volume.FileBlockStorage.h>
#include <niven.Volume.MarchingCubes.h>
#include <niven.Volume.Volume.h>

#include <niven.Engine.BaseApplication3D.h>
#include <niven.Engine.DebugRenderUtility.h>

#include <niven.Render.EffectLoader.h>
#include <niven.Render.EffectManager.h>

#include <niven.Core.Math.VectorToString.h>

#include <niven.Engine.Draw2D.h>
#include <niven.Engine.Draw2DRectangle.h>

#include <niven.Core.IO.FileSystem.h>
#include <niven.Core.Log.h>
#include <niven.Core.StringFormat.h>
#include <niven.Engine.Render.EffectLoader.h>
#include <niven.Core.Stopwatch.h>
#include <niven.Engine.Render.DrawCommand.h>
#include <niven.Core.IO.File.h>
#include <niven.Image.Image2D.h>
#include <niven.Image.Image2D_All.h>
#include <niven.Image.ImageLoadHelper.h>
#include <niven.Engine.Render.Texture2D.h>
#include <niven.Core.Math.Matrix.h>
#include <niven.Core.Math.MatrixFunctions.h>
#include <niven.Engine.Interop.ObjReader.h>

#include <niven.Core.IO.Path.h>

#include <niven.Render.Effect.h>
#include <niven.Engine.Render.EffectManager.h>
#include <niven.Render.RenderContext.h>
#include <niven.Render.Texture.h>
#include <niven.Render.VertexLayout.h>
#include <niven.Render.VertexBuffer.h>
#include <niven.Render.IndexBuffer.h>

#include <niven.Engine.Geometry.SimpleMesh.h>
#include <niven.Engine.Spatial.AxisAlignedBoundingBox.h>

#include <set>
#include <boost/container/flat_map.hpp>

#include <memory>

#include "materialLibrary.h"

#include <vector>
#include <memory>

#include <iostream>

#if defined(NIV_DEBUG) && defined(NIV_OS_WINDOWS)
#define StartMemoryDebugging() \
	_CrtMemState __initialState; \
	_CrtMemCheckpoint(&__initialState); \
	_CrtMemDumpStatistics	(&__initialState); \
	_ASSERTE( _CrtCheckMemory( ) )
#define StopMemoryDebugging() \
	_ASSERTE( _CrtCheckMemory( ) );	\
	_CrtMemDumpAllObjectsSince(&__initialState)
#else
#define StartMemoryDebugging()
#define StopMemoryDebugging()
#endif

using namespace niven;

int main (int /* argc */, char* /* argv */ [])
{
	StartMemoryDebugging();
	{
#define EXC
		//#undef EXC
#ifdef EXC
		try
#endif
		{
			Core::Initialize ();

			Log::Info ("VolumePlacerUI","Log started");

			auto objPath = IO::Path( "P:\\BlenderScenes\\two_boxes.obj" );
			IO::Path baseDirectory = objPath.GetParentDirectory();

			Log::Info ("ObjModel", String::Format ("Loading obj file '{0}'") % objPath );
			Stopwatch stopWatch;

			Interop::Obj::Reader reader;
			reader.Load (*FileSystem::OpenFile (objPath));
			Log::Info ("ObjModel", String::Format (" ... {0} seconds") % stopWatch.GetElapsedTime ());

			MaterialLibrary materialLibrary;
			for( int i = 0 ; i < (int) reader.GetMaterialLibraries().size() ; i++ ) {
				const String mtl = reader.GetMaterialLibraries()[i];
				IO::Path mtlPath = baseDirectory / mtl;

				Log::Info ("ObjModel", String::Format ("Loading mtllib '{0}'") % mtlPath );

				MaterialLibrary::LoadMTL( *FileSystem::OpenFile( mtlPath ), materialLibrary );
			}

			std::vector<SimpleMesh::Ptr> meshes;
			for (int i = 0; i < reader.GetChunkCount (); ++i)
			{
				const Interop::Obj::Chunk *chunk = reader.GetChunk(i);
				SimpleMesh::Ptr mesh = ConvertToSimpleMesh (chunk);
				meshes.push_back( mesh );
			}

			// global stats
			// ensure that all primitive types are equal and start to count everything...
			std::vector<int> vertexOffsets;
			std::vector<int> indexOffsets;

			PrimitiveType::Enum primitiveType = meshes[0]->GetPrimitiveType();
			const char *vertexFormat = meshes[0]->GetVertexFormat().GetId();

			vertexOffsets.push_back(0);
			indexOffsets.push_back(0);
			for (int i = 0; i < reader.GetChunkCount (); ++i) {
				vertexOffsets.push_back( vertexOffsets.back() + meshes[i]->GetVertexCount() );
				indexOffsets.push_back( indexOffsets.back() + meshes[i]->GetIndexCount() );

				if( meshes[i]->GetPrimitiveType() != primitiveType ) {
					// error: wrong primitive type
					Log::Error( "modelExporter", "inconsistent primitive type" );
					return -1;
				}
				if( meshes[i]->GetVertexFormat().GetId() != vertexFormat ) {
					// error: wrong primitive type
					Log::Error( "modelExporter", "inconsistent vertex format" );
				}
			}

			Log::Info ("VolumePlacerUI","Log closed");

			Core::Shutdown ();
		}
#ifdef EXC
		catch (Exception& e)
		{
			std::cout << e.what () << std::endl;
			std::cerr << e.GetDetailMessage() << std::endl;
			std::cout << e.where () << std::endl;
		}
		catch (std::exception& e)
		{
			std::cout << e.what () << std::endl;
		}
#endif
	}
	StopMemoryDebugging();

	return 0;
}