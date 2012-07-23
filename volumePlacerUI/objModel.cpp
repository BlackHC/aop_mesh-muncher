#include <niven.Core.Core.h>

#include "objModel.h"

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

using namespace niven;

void ObjModel::Init( niven::IRenderSystem::Ptr renderSystem, niven::Render::EffectManager &effectManager, niven::IO::Path &objPath ) {
	// create null texture
	nullTexture_ = renderSystem->Wrap( renderSystem->CreateTexture2D( 1, 1, 1, PixelFormat::R8G8B8A8 ) );

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

	std::map<String, Render::ITexture::Ptr> textureMap;
	for (int i = 0; i < reader.GetChunkCount (); ++i)
	{
		const Interop::Obj::Chunk *chunk = reader.GetChunk(i);
		meshes_.push_back (ConvertToSimpleMesh (chunk));

		// load material
		textures_.push_back( nullptr );

		const String &materialName = chunk->GetMaterial();
		if( !materialName.IsEmpty() ) {
			Log::Info( "ObjModel", String::Format( "Chunk {0} uses material {1}" ) % i % materialName );

			MaterialLibrary::Material &material = materialLibrary.materialMap[ materialName ];
			if( !material.texture.IsEmpty() ) {
				// has the texture been loaded?
				auto &texture = textureMap[material.texture];
				if( texture ) {
					// okay, reuse it
					textures_.push_back( texture );
				}
				else {
					// try to load it
					const IO::Path imagePath = baseDirectory / material.texture;
					// TODO: better fail checks..
					if( imagePath.GetExtension() == ".avi" ) {
						continue;
					}

					try {
						Image::Image2D::Ptr image = Image::LoadImage<Image2D_4b>( imagePath, LoadImageFlags::AutoConvert );								
						Render::ITexture::Ptr texture = Render::ITexture::Ptr( renderSystem->Wrap( renderSystem->CreateTexture2D( Render::Texture2DDescriptor( *image ) ) ) );

						textureMap[ material.texture ] = texture;
						textures_.back() = texture;
					}
					catch(...) {
						Log::Info( "ObjModel", String::Format( "Could not load texture {0}" ) % imagePath );
					}
				}
			}
		}
	}

	const int meshCount = static_cast<int>(meshes_.size ());
	for (int i = 0; i < meshCount; ++i)
	{
		SimpleMesh *mesh = meshes_[i].get();

		Log::Info ("ObjModel", 
			String::Format ("Uploading mesh {0}/{1} with vertex format {2}") % (i+1) % meshCount % mesh->GetVertexFormat ().GetId () );

		effects_.push_back (effectManager.GetEffect ("VolumePlacerUI", mesh->GetVertexFormat ().GetId ()));

		vertexLayouts_.push_back (renderSystem->Wrap (renderSystem->CreateVertexLayout (mesh->GetVertexFormat ().GetVertexLayout(),	effects_ [i]->GetVertexShaderProgram ())));

		vertexBuffers_.push_back(renderSystem->Wrap (renderSystem->CreateVertexBuffer (
			mesh->GetVertexFormat ().GetSize (), 
			mesh->GetVertexCount (),
			Render::ResourceUsage::Static,
			mesh->GetVertexDataPointer ())));

		indexBuffers_.push_back (renderSystem->Wrap (renderSystem->CreateIndexBuffer (
			IndexBufferFormat::UInt_32, 
			mesh->GetIndexCount (),
			Render::ResourceUsage::Static,
			mesh->GetIndexDataPointer ())));
	}
}

void ObjModel::Draw( niven::Render::IRenderContext *renderContext ) {
	Matrix4f worldView = renderContext->GetWorld() * renderContext->GetView();

	for (int i = 0; i < static_cast<int> (indexBuffers_.size ()); ++i)
	{
		DrawIndexedCommand dic;
		dic.SetIndexBuffer (indexBuffers_ [i]);
		dic.SetVertexBuffers (1, &vertexBuffers_ [i]);
		dic.SetVertexLayout (vertexLayouts_ [i]);
		dic.type			= meshes_[i]->GetPrimitiveType ();
		dic.indexCount		= meshes_[i]->GetIndexCount ();
		dic.vertexCount		= meshes_[i]->GetVertexCount ();

		if( textures_[ i ] ) {
			effects_[i]->SetTexture( "Diffuse_Texture", textures_[i] );
		}
		else {
			effects_[i]->SetTexture( "Diffuse_Texture", nullTexture_ );
		}

		effects_[i]->SetMatrix( "WorldView", worldView );

		effects_ [i]->Bind (renderContext);
		renderContext->Draw (dic);
		effects_ [i]->Unbind (renderContext);
	}
}