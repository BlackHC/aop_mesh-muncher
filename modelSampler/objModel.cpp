#include "objModel.h"

#include <niven.Core.Core.h>

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

void ObjSceneGL::Init( const char *objFile ) {
	niven::IO::Path objPath( objFile );
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

	std::vector<SimpleMesh::Ptr> simpleMeshes;
	std::map<String, GLuint> textureMap;
	for (int i = 0; i < reader.GetChunkCount (); ++i)
	{
		const Interop::Obj::Chunk *chunk = reader.GetChunk(i);
		{
			SubModel subModel;
			simpleMeshes.push_back( ConvertToSimpleMesh (chunk) );
			subModel.texture = 0;

			subModel.objectName = chunk->GetObjectName().ToStdString();
			std::transform( chunk->GetGroups().cbegin(), chunk->GetGroups().cend(), std::inserter( subModel.groupNames, subModel.groupNames.begin() ), [] (const niven::String &str) { return str.ToStdString(); } );

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
			subModels.back().diffuseColor[0] = material.diffuseColor[0]; subModels.back().diffuseColor[1] = material.diffuseColor[1]; subModels.back().diffuseColor[2] = material.diffuseColor[2];

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
						GLuint texture;
						glGenTextures( 1, &texture );
						glBindTexture( GL_TEXTURE_2D, texture );
						glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, image->GetWidth(), image->GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image->GetData() );
						glGenerateMipmap( GL_TEXTURE_2D );
						glBindTexture( GL_TEXTURE_2D, 0 );

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
		SimpleMesh *simpleMesh = simpleMeshes[i].get();
		SubModel &subModel = subModels[i];


		Log::Info ("ObjModel", 
			String::Format ("Uploading mesh {0}/{1} with vertex format {2}") % (i+1) % meshCount % simpleMesh->GetVertexFormat ().GetId () );

		subModel.displayList = glGenLists( 1 );
		glNewList( subModel.displayList, GL_COMPILE );

		size_t vertexSize = simpleMesh->GetVertexFormat().GetSize();
		auto vertexComponents = simpleMesh->GetVertexFormat().GetVertexLayout();

		glEnableClientState( GL_VERTEX_ARRAY );
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );

		for( auto vertexComponent = vertexComponents.begin() ; vertexComponent != vertexComponents.end() ; ++vertexComponent ) {
			GLuint componentType, componentSize;
			switch( vertexComponent->GetElementType() ) {
			case VertexElementType::Float_3:
				componentType = GL_FLOAT;
				componentSize = 3;
				break;
			case VertexElementType::Float_2:
				componentType = GL_FLOAT;
				componentSize = 2;
				break;
			default:
				// error
				__debugbreak();
			}
			switch( vertexComponent->GetSemantic() ) {
			case VertexElementSemantic::Position:
				glVertexPointer( componentSize, componentType, vertexSize, (const char*) simpleMesh->GetVertexDataPointer() + vertexComponent->GetOffset() );
				break;
			case VertexElementSemantic::UV:
				glTexCoordPointer( componentSize, componentType, vertexSize, (const char*) simpleMesh->GetVertexDataPointer() + vertexComponent->GetOffset() );
				glEnableClientState( GL_TEXTURE_COORD_ARRAY );
				break;
			default:
				// error
				__debugbreak();
			}
		}

		glBindTexture( GL_TEXTURE_2D, subModel.texture );
		if( subModel.texture ) {
			glEnable( GL_TEXTURE_2D );
		}
		else {
			glDisable( GL_TEXTURE_2D );
		}

		GLuint primitiveType;
		switch( simpleMesh->GetPrimitiveType() ) {
		case PrimitiveType::Triangle_List:
			primitiveType = GL_TRIANGLES;
			break;
		}

		glColor3fv( subModel.diffuseColor );

		glDrawElements( primitiveType, simpleMesh->GetIndexCount(), GL_UNSIGNED_INT, simpleMesh->GetIndexDataPointer() );

		glEndList();
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

		Eigen::AlignedBox3f subBB;
		subBB.min() = Eigen::Map<Eigen::Vector3f>( &simpleMeshes[i]->GetBoundingBox().GetMinimum().X() );
		subBB.max() = Eigen::Map<Eigen::Vector3f>( &simpleMeshes[i]->GetBoundingBox().GetMaximum().X() );

		model->boundingBox.extend( subBB );	
		boundingBox.extend( subBB );
	}
}

void ObjSceneGL::Draw() {
	for (int i = 0; i < static_cast<int> (subModels.size ()); ++i)
	{
		SubModel &subModel = subModels[i];

		if( !subModel.visible ) {
			continue;
		}

		glCallList( subModel.displayList );
	}
}

void ObjSceneGL::SetObjectVisibility( const std::string &objectName, bool visible )
{
	for( auto subModel = subModels.begin() ; subModel != subModels.end() ; ++subModel ) {
		if( subModel->objectName == objectName ) {
			subModel->visible = visible;
		}
	}
}

void ObjSceneGL::SetGroupVisibilty( const std::string &groupName, bool visible )
{
	for( auto subModel = subModels.begin() ; subModel != subModels.end() ; ++subModel ) {
		if( subModel->groupNames.count( groupName ) ) {
			subModel->visible = visible;
		}
	}
}
