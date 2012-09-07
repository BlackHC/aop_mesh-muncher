#pragma once

#include "sgsScene.h"

#include "gl/glew.h"

#include "SOIL.h"

struct SGSSceneRenderer {
	GLuint displayListBase;
	std::vector<GLuint> textureHandles;

	void processScene( const SGSScene &scene ) {
		// load textures
		int numTextures = scene.textures.size();
		textureHandles.resize( numTextures );
		glGenTextures( numTextures, &textureHandles.front() );

		for( int textureIndex = 0 ; textureIndex < numTextures ; ++textureIndex ) {
			const auto &rawContent = scene.textures[ textureIndex ].rawContent;
			SOIL_load_OGL_texture_from_memory( &rawContent.front(), rawContent.size(), 0, textureHandles[ textureIndex ], SOIL_FLAG_DDS_LOAD_DIRECT | SOIL_FLAG_MIPMAPS | SOIL_FLAG_TEXTURE_REPEATS );
		}

		// render everything into display lists
		displayListBase = glGenLists( 1 );
		
		glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS );

		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_NORMAL_ARRAY );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );

		glNewList( displayListBase, GL_COMPILE );
			
		glPushAttrib( GL_ALL_ATTRIB_BITS );

		{
			auto &firstVertex = scene.vertices[0];
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.uv[0] );
		
			//glDrawElements( GL_TRIANGLES, scene.indices.size(), GL_UNSIGNED_INT, &scene.indices.front() );

			glEnable( GL_TEXTURE_2D );

			for( auto subObjectIterator = scene.subObjects.begin() ; subObjectIterator != scene.subObjects.end() ; ++subObjectIterator ) {
				const SGSScene::SubObject &subObject = *subObjectIterator;

				// apply the material
				const auto &material = subObject.material;
				glColor3ubv( &material.diffuse.r );
			
				if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
					glBindTexture( GL_TEXTURE_2D, textureHandles[ material.textureIndex[0] ] );
				}
				else {
					glBindTexture( GL_TEXTURE_2D, 0 );
				}

				glDrawElements( GL_TRIANGLES, subObject.numIndices, GL_UNSIGNED_INT, &scene.indices.front() + subObject.startIndex );
			}
		}

		// render terrain
		{
			auto &firstVertex = scene.terrain.vertices[0];
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.blendUV );

			glBindTexture( GL_TEXTURE_2D, 0 );
			glDisable( GL_TEXTURE_2D );

			glColor3f( 0.5, 0.5, 0.5 );
			glDrawElements( GL_TRIANGLES, scene.terrain.indices.size(), GL_UNSIGNED_INT, &scene.terrain.indices.front() );
		}

		glPopAttrib();
		
		glEndList();

		glPopClientAttrib();
	}

	void render() {
		glCallList( displayListBase );
	}

	SGSSceneRenderer() : displayListBase( 0 ) {}
	~SGSSceneRenderer() {
		if( displayListBase )
			glDeleteLists( displayListBase, 1 );
	}
};
