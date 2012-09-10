#pragma once

#include "sgsScene.h"

#include "gl/glew.h"

#include "SOIL.h"

struct Renderbuffer {
	GLuint handle;

	Renderbuffer( GLenum internalFormat, int width, int height ) {
		glGenRenderbuffers( 1, &handle );
		glNamedRenderbufferStorageEXT( handle, internalFormat, width, height );
	}

	~Renderbuffer() {
		glDeleteRenderbuffers( 1, &handle );
	}
};

struct Texture {
	GLuint handle;

	Texture( GLuint handle ) : handle( handle ) {}
};

template< class SpecializedTexture = Texture >
struct ScopedTexture : SpecializedTexture {
	static GLuint createTextureHandle() {
		GLuint handle;
		glGenTextures( 1, &handle );
		return handle;
	}

	ScopedTexture() : SpecializedTexture( createTextureHandle() ) {}
	~ScopedTexture() {
		if( handle ) {
			glDeleteTextures( 1, &handle );
		}
	}

	SpecializedTexture publish() {
		SpecializedTexture globalTexture( handle );
		handle = 0;
		return globalTexture;
	}
};

struct Texture2D {
	static GLuint currentHandle;

	GLuint handle;

	Texture2D( GLuint handle ) : handle( handle ) {}

	void bind() {
		if( currentHandle != handle ) {
			currentHandle = handle;
			glBindTexture( GL_TEXTURE_2D, handle );
		}
	}

	static void unbind() {
		if( currentHandle ) {
			glBindTexture( GL_TEXTURE_2D, 0 );
			currentHandle = 0;
		}
	}

	void load( GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels = nullptr ) {
		glTextureImage2DEXT( handle, GL_TEXTURE_2D, level, internalformat, width, height, border, format, type, pixels );
	}

	void immutable( int numLevels, GLenum internalFormat, GLsizei width, GLsizei height ) {
		if( handle != currentHandle ) {
			glBindTexture( GL_TEXTURE_2D, handle );
			glTexStorage2D( GL_TEXTURE_2D, numLevels, internalFormat, width, height );
			glBindTexture( GL_TEXTURE_2D, currentHandle );
		}
		else {
			glTexStorage2D( GL_TEXTURE_2D, numLevels, internalFormat, width, height );
		}
	}

	static void enable() {
		glEnable( GL_TEXTURE_2D );
	}

	static void disable() {
		glDisable( GL_TEXTURE_2D );
	}

	void generateMipmap() {
		glGenerateTextureMipmapEXT( handle, GL_TEXTURE_2D );
	}

	static void bindExtern( GLuint handle ) {
		if( handle == currentHandle ) {
			return;
		}

		currentHandle = handle;
		glBindTexture( GL_TEXTURE_2D, handle );
	}
};

GLuint Texture2D::currentHandle = 0;

template< class SpecializedTexture = Texture >
struct Textures {
	std::vector<GLuint> handles;

	Textures( int size = 0 ) : handles( size ) {}

	SpecializedTexture get( int index ) {
		return SpecializedTexture( handles[ index ] );
	}
};

template< class SpecializedTexture = Texture >
struct ScopedTextures : Textures< SpecializedTexture > {
	ScopedTextures( int size ) : Textures( size ) { 
		glGenTextures( size, &handles.front() );
	}

	~ScopedTextures() {
		glDeleteTextures( handles.size(), &handles.front() );
	}
	
	SpecializedTexture publish( int index ) {
		SpecializedTexture globalTexture( handles[ index ] );
		handles[ index ] = 0;
		return globalTexture;
	}

	Textures<SpecializedTexture> publish() {
		Textures<SpecializedTexture> globalTextures;

		std::swap( globalTextures.handles, handles );

		return globalTextures;
	}
};

typedef ScopedTexture< Texture2D > ScopedTexture2D;
typedef ScopedTextures< Texture2D > ScopedTextures2D;

struct Framebuffer {
	GLuint handle;
	bool colorAttachment0;
	bool depthAttachment;

	Framebuffer() : handle( 0 ), depthAttachment( false ), colorAttachment0( false ) {
		glGenFramebuffers( 1, &handle );
	}

	~Framebuffer() {
		if( handle ) {
			glDeleteFramebuffers( 1, &handle );
		}
	}

	void bind() {
		glBindFramebuffer( GL_FRAMEBUFFER, handle );
	}

	static void unbind() {
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	void attach( const Renderbuffer &renderbuffer, GLenum attachment ) {
		glNamedFramebufferRenderbufferEXT( handle, attachment, GL_RENDERBUFFER, renderbuffer.handle );
		changeAttachmentState( attachment, true );
	}

	void attach( const Texture2D &texture, GLenum attachment, int level = 0 ) {
		glNamedFramebufferTexture2DEXT( handle, attachment, GL_TEXTURE_2D, texture.handle, level );
		changeAttachmentState( attachment, true );
	}

	void detach( GLenum attachment ) {
		glNamedFramebufferRenderbufferEXT( handle, attachment, GL_RENDERBUFFER, 0 );
		changeAttachmentState( attachment, false );
	}

	void changeAttachmentState( GLenum attachment, bool state ) {
		switch( attachment ) {
		case GL_DEPTH_ATTACHMENT:
			depthAttachment = state;
			break;
		case GL_COLOR_ATTACHMENT0:
			colorAttachment0 = state;
			break;
		}
	}

	void setDrawBuffers() {
		GLenum drawBuffers[1];
		int i = 0;

		if( colorAttachment0 ) {
			drawBuffers[i++] = GL_COLOR_ATTACHMENT0;
		}

		if( i > 0 ) {
			glDrawBuffers( i, drawBuffers );
		}
		else {
			glDrawBuffer( GL_NONE );
		}
	}
};

struct SGSSceneRenderer {
	std::shared_ptr<SGSScene> scene;

	GLuint subObjects_disptlayListBase;
	GLuint terrain_displayListBase;

	std::vector<GLuint> solidLists;
	std::vector<GLuint> alphaLists;

	std::vector<GLuint> textureHandles;

	Texture2D bakeTerrainTexture( int detailFactor, float textureDetailFactor ) {
		glPushAttrib( GL_ALL_ATTRIB_BITS );
		
		int numLayers = scene->terrain.layers.size();

		// create and load weight textures
		ScopedTextures2D weightTextures( numLayers );

		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

		for( int layer = 0 ; layer < numLayers ; ++layer ) {
			auto weightTexture = weightTextures.get( layer );
			weightTexture.bind();
			weightTexture.load( 0, GL_INTENSITY, scene->terrain.layerSize[0], scene->terrain.layerSize[1], 0, GL_RED, GL_UNSIGNED_BYTE, &scene->terrain.layers[ layer ].weights.front() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		}

		Framebuffer fbo;
		ScopedTexture2D bakedTexture;

		Eigen::Vector2i mapSize( scene->terrain.mapSize[0], scene->terrain.mapSize[1] );
		Eigen::Vector2i bakeSize = mapSize * detailFactor;

		bakedTexture.load( 0, GL_RGBA8, bakeSize.x(), bakeSize.y(), 0, GL_RGBA, GL_UNSIGNED_BYTE );
		
		fbo.attach( bakedTexture, GL_COLOR_ATTACHMENT0 );

		fbo.bind();
		fbo.setDrawBuffers();

		glViewport( 0, 0, bakeSize.x(), bakeSize.y() );

		// no depth buffer
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );

		glClearColor( 0.0, 0.0, 0.0, 0.0 );
		glClear( GL_COLOR_BUFFER_BIT );

		// enable blending
		glEnable( GL_BLEND );
		// additive blending
		glBlendFunc( GL_ONE, GL_ONE );

		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();

		// set the combiners
		glActiveTexture( GL_TEXTURE0 );
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		glActiveTexture( GL_TEXTURE1 );
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		glMatrixMode( GL_TEXTURE );
		glScalef( textureDetailFactor * mapSize.x(), textureDetailFactor * mapSize.y(), 1.0 );

		for( int layerIndex = 0 ; layerIndex < numLayers ; ++layerIndex ) {
			// bind weight first
			glActiveTexture( GL_TEXTURE0 );
			auto weightTexture = weightTextures.get( layerIndex );
			weightTexture.bind();
			weightTexture.enable();
			
			// bind the terrain texture
			glActiveTexture( GL_TEXTURE1 );
			Texture2D terrainTexture( textureHandles[ scene->terrain.layers[ layerIndex ].textureIndex ] );
			terrainTexture.bind();
			terrainTexture.enable();
			
			glBegin( GL_QUADS );
				glMultiTexCoord2f( GL_TEXTURE0, 0.0, 0.0 );
				glMultiTexCoord2f( GL_TEXTURE1, 0.0, 0.0 );
				glVertex3f( -1.0, -1.0, 0.0 );

				glMultiTexCoord2f( GL_TEXTURE0, 1.0, 0.0 );
				glMultiTexCoord2f( GL_TEXTURE1, 1.0, 0.0 );
				glVertex3f( 1.0, -1.0, 0.0 );

				glMultiTexCoord2f( GL_TEXTURE0, 1.0, 1.0 );
				glMultiTexCoord2f( GL_TEXTURE1, 1.0, 1.0 );
				glVertex3f( 1.0, 1.0, 0.0 );

				glMultiTexCoord2f( GL_TEXTURE0, 0.0, 1.0 );
				glMultiTexCoord2f( GL_TEXTURE1, 0.0, 1.0 );
				glVertex3f( -1.0, 1.0, 0.0 );
			glEnd();
		}

		glActiveTexture( GL_TEXTURE0 );
		
		fbo.unbind();

		glPopAttrib();

		bakedTexture.generateMipmap();

		return bakedTexture.publish();
	}

	void processScene( const std::shared_ptr<SGSScene> &scene ) {
		this->scene = scene;

		// load textures
		int numTextures = scene->textures.size();
		textureHandles.resize( numTextures );
		glGenTextures( numTextures, &textureHandles.front() );

		for( int textureIndex = 0 ; textureIndex < numTextures ; ++textureIndex ) {
			const auto &rawContent = scene->textures[ textureIndex ].rawContent;
			SOIL_load_OGL_texture_from_memory( &rawContent.front(), rawContent.size(), 0, textureHandles[ textureIndex ], SOIL_FLAG_DDS_LOAD_DIRECT | SOIL_FLAG_MIPMAPS | SOIL_FLAG_TEXTURE_REPEATS );
			glBindTexture( GL_TEXTURE_2D, textureHandles[ textureIndex ] );
		}

		Texture2D bakedTerrainTexture = bakeTerrainTexture( 32, 1 / 8.0 );

		// render everything into display lists
		subObjects_disptlayListBase = glGenLists( scene->subObjects.size() );

		terrain_displayListBase = glGenLists( 1 );

		solidLists.reserve( scene->subObjects.size() );
		alphaLists.reserve( scene->subObjects.size() );
		
		glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS );

		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_NORMAL_ARRAY );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );

		glPushAttrib( GL_ALL_ATTRIB_BITS );

		solidLists.push_back( terrain_displayListBase );

		{
			auto &firstVertex = scene->vertices[0];
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.uv[0] );
		
			for( int subObjectIndex = 0 ; subObjectIndex < scene->subObjects.size() ; ++subObjectIndex ) {
				const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

				glNewList( subObjects_disptlayListBase + subObjectIndex, GL_COMPILE );

				glEnable( GL_TEXTURE_2D );

				// apply the material
				const auto &material = subObject.material;
				
				if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
					glBindTexture( GL_TEXTURE_2D, textureHandles[ material.textureIndex[0] ] );
				}
				else {
					glBindTexture( GL_TEXTURE_2D, 0 );
				}

				unsigned char alpha = 255;

				switch( material.alphaType ) {
				default:
				case SGSScene::Material::AT_NONE:
					glDisable( GL_BLEND );
					glDisable( GL_ALPHA_TEST );

					glDepthMask( GL_TRUE ); 
					
					break;
				case SGSScene::Material::AT_ADDITIVE:
					glDepthMask( GL_FALSE );
					glEnable( GL_BLEND );
					glDisable( GL_ALPHA_TEST );

					glBlendFunc( GL_SRC_ALPHA, GL_ONE );

					break;
				case SGSScene::Material::AT_TEXTURE:
				case SGSScene::Material::AT_ALPHATEST:
					glDepthMask( GL_FALSE );
					glEnable( GL_ALPHA_TEST );
					glAlphaFunc( GL_GREATER, 0 );

					glEnable( GL_BLEND );
					glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

					alpha = material.alpha;

					break;
				case SGSScene::Material::AT_MATERIAL:
					if( material.alpha == 255 ) {
						glDisable( GL_BLEND );
						glDisable( GL_ALPHA_TEST );

						glDepthMask( GL_TRUE ); 
					}
					else {
						glDepthMask( GL_FALSE );
						glDisable( GL_ALPHA_TEST );

						glEnable( GL_BLEND );
						glBlendFunc( GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA );

						glBlendColor( 1.0, 1.0, 1.0, material.alpha / 255.0 );
					}
					break;
				case SGSScene::Material::AT_MULTIPLY:
					glDepthMask( GL_FALSE );
					glDisable( GL_ALPHA_TEST );
					
					glEnable( GL_BLEND );
					glBlendFunc( GL_ZERO, GL_SRC_COLOR );

					break;
				case SGSScene::Material::AT_MULTIPLY_2:
					glDepthMask( GL_FALSE );
					glDisable( GL_ALPHA_TEST );

					glEnable( GL_BLEND );
					glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );

					break;
				}

				glColor4ub( material.diffuse.r, material.diffuse.g, material.diffuse.b, alpha );

				glDrawElements( GL_TRIANGLES, subObject.numIndices, GL_UNSIGNED_INT, &scene->indices.front() + subObject.startIndex );

				glEndList();

				if( material.alphaType == SGSScene::Material::AT_NONE ) {
					solidLists.push_back( subObjects_disptlayListBase + subObjectIndex );
				} 
				else {
					alphaLists.push_back( subObjects_disptlayListBase + subObjectIndex );
				}
			}
		}

		// render terrain
		glNewList( terrain_displayListBase, GL_COMPILE );
		{
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );

			glDepthMask( GL_TRUE );

			auto &firstVertex = scene->terrain.vertices[0];
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.blendUV );

			bakedTerrainTexture.bind();
			bakedTerrainTexture.enable();

			glColor3f( 1.0, 1.0, 1.0 );
			glDrawElements( GL_TRIANGLES, scene->terrain.indices.size(), GL_UNSIGNED_INT, &scene->terrain.indices.front() );
		}
		glEndList();

		glPopAttrib();
		
		glPopClientAttrib();
	}

	void render() {
		if( !solidLists.empty() )
			glCallLists( solidLists.size(), GL_UNSIGNED_INT, &solidLists.front() );
		if( !alphaLists.empty() )
			glCallLists( alphaLists.size(), GL_UNSIGNED_INT, &alphaLists.front() );

		glDepthMask( GL_TRUE );
	}

	SGSSceneRenderer() : subObjects_disptlayListBase( 0 ), terrain_displayListBase( 0 ) {}

	~SGSSceneRenderer() {
		if( subObjects_disptlayListBase ) {
			glDeleteLists( subObjects_disptlayListBase, scene->subObjects.size() );
		}
		if( terrain_displayListBase ) {
			glDeleteLists( terrain_displayListBase, scene->subObjects.size() );
		}
	}
};
