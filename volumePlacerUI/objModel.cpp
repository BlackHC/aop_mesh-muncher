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

void ObjScene::Init( niven::IRenderSystem::Ptr renderSystem, niven::Render::EffectManager &effectManager, niven::IO::Path &objPath ) {
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
		{
			SubModel subModel;
			subModel.mesh = ConvertToSimpleMesh (chunk);
			subModel.texture = nullptr;

			subModel.objectName = chunk->GetObjectName();
			subModel.groupNames = chunk->GetGroups();

			subModel.visible = true;

			subModels.push_back( std::move( subModel ) );

			objectNames.insert( subModel.objectName );
			groupNames.insert( subModel.groupNames.cbegin(), subModel.groupNames.cend() );
		}

		// load material
		const String &materialName = chunk->GetMaterial();
		if( !materialName.IsEmpty() ) {
			Log::Info( "ObjModel", String::Format( "Chunk {0} uses material {1}" ) % i % materialName );

			MaterialLibrary::Material &material = materialLibrary.materialMap[ materialName ];
			if( !material.texture.IsEmpty() ) {
				// has the texture been loaded?
				auto &texture = textureMap[material.texture];
				if( texture ) {
					// okay, reuse it
					subModels.back().texture = texture;
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
						subModels.back().texture = texture;
					}
					catch(...) {
						Log::Info( "ObjModel", String::Format( "Could not load texture {0}" ) % imagePath );
					}
				}
			}
		}
	}

	const int meshCount = static_cast<int>(subModels.size ());
	for (int i = 0; i < meshCount; ++i)
	{
		SubModel &subModel = subModels[i];

		Log::Info ("ObjModel", 
			String::Format ("Uploading mesh {0}/{1} with vertex format {2}") % (i+1) % meshCount % subModel.mesh->GetVertexFormat ().GetId () );

		subModel.effect = effectManager.GetEffect ("VolumePlacerUI", subModel.mesh->GetVertexFormat ().GetId ());

		subModel.vertexLayout = renderSystem->Wrap (renderSystem->CreateVertexLayout (subModel.mesh->GetVertexFormat ().GetVertexLayout(),	subModel.effect->GetVertexShaderProgram ()));

		subModel.vertexBuffer = renderSystem->Wrap (renderSystem->CreateVertexBuffer (
			subModel.mesh->GetVertexFormat ().GetSize (), 
			subModel.mesh->GetVertexCount (),
			Render::ResourceUsage::Static,
			subModel.mesh->GetVertexDataPointer ()));

		subModel.indexBuffer = renderSystem->Wrap (renderSystem->CreateIndexBuffer (
			IndexBufferFormat::UInt_32, 
			subModel.mesh->GetIndexCount (),
			Render::ResourceUsage::Static,
			subModel.mesh->GetIndexDataPointer ()));
	}

	// fill the models vector
	models.reserve( objectNames.size() );
	
	for( auto it = objectNames.begin() ; it != objectNames.end() ; ++it ) {
		models.push_back( Model() );
		models.back().name = *it;

		nameModelMap.insert( std::make_pair( *it, &models.back() ) );
	}

	for( int i = 0 ; i < subModels.size() ; ++i ) {
		Model *model = nameModelMap[ subModels[i].objectName ];
		model->subModels.push_back( &subModels[i] );

		const niven::AxisAlignedBoundingBox3 &subBB = subModels[i].mesh->GetBoundingBox();
		// TODO: is this really necessary or would it work automatically?
		if( !model->boundingBox.IsEmpty() ) {
			model->boundingBox.Merge( subBB );
		}
		else {
			model->boundingBox = subBB;
		}

		if( !boundingBox.IsEmpty() ) {
			boundingBox.Merge( subBB );
		} 
		else {
			boundingBox = subBB;
		}
	}
}

void ObjScene::Draw( niven::Render::IRenderContext *renderContext ) {
	Matrix4f worldView = renderContext->GetWorld() * renderContext->GetView();

	for (int i = 0; i < static_cast<int> (subModels.size ()); ++i)
	{
		SubModel &subModel = subModels[i];

		if( !subModel.visible ) {
			continue;
		}

		DrawIndexedCommand dic;
		dic.SetIndexBuffer (subModel.indexBuffer);
		dic.SetVertexBuffers (1, &subModel.vertexBuffer);
		dic.SetVertexLayout (subModel.vertexLayout);
		dic.type			= subModel.mesh->GetPrimitiveType ();
		dic.indexCount		= subModel.mesh->GetIndexCount ();
		dic.vertexCount		= subModel.mesh->GetVertexCount ();

		if( subModel.texture ) {
			subModel.effect->SetTexture( "Diffuse_Texture", subModel.texture );
		}
		else {
			subModel.effect->SetTexture( "Diffuse_Texture", nullTexture_ );
		}

		subModel.effect->SetMatrix( "WorldView", worldView );

		subModel.effect->Bind (renderContext);
		renderContext->Draw (dic);
		subModel.effect->Unbind (renderContext);
	}
}

void ObjScene::SetObjectVisibility( const niven::String &objectName, bool visible )
{
	for( auto subModel = subModels.begin() ; subModel != subModels.end() ; ++subModel ) {
		if( subModel->objectName == objectName ) {
			subModel->visible = visible;
		}
	}
}

void ObjScene::SetGroupVisibilty( const niven::String &groupName, bool visible )
{
	for( auto subModel = subModels.begin() ; subModel != subModels.end() ; ++subModel ) {
		if( subModel->groupNames.count( groupName ) ) {
			subModel->visible = visible;
		}
	}
}
