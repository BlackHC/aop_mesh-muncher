#pragma once

#include "sgsScene.h"

#include "gl/glew.h"

#include "SOIL.h"

#include "glslPipeline.h"
#include <boost/range/algorithm/sort.hpp>
#include <debugRender.h>

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

	Texture( GLuint handle = 0 ) : handle( handle ) {}
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
	GLuint handle;

	Texture2D( GLuint handle = 0 ) : handle( handle ) {}

	void bind() {
		glBindTexture( GL_TEXTURE_2D, handle );
	}

	static void unbind() {
		glBindTexture( GL_TEXTURE_2D, 0 );
	}

	void load( GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels = nullptr ) {
		glTextureImage2DEXT( handle, GL_TEXTURE_2D, level, internalformat, width, height, border, format, type, pixels );
	}

	void immutable( int numLevels, GLenum internalFormat, GLsizei width, GLsizei height ) {
		throw std::logic_error( "broken" );
#if 0	
		{
			glBindTexture( GL_TEXTURE_2D, handle );
			glTexStorage2D( GL_TEXTURE_2D, numLevels, internalFormat, width, height );
			glBindTexture( GL_TEXTURE_2D, currentHandle );
		}
		else {
			glTexStorage2D( GL_TEXTURE_2D, numLevels, internalFormat, width, height );
		}
#endif
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
		glBindTexture( GL_TEXTURE_2D, handle );
	}
};

template< class SpecializedTexture = Texture >
struct Textures {
	std::vector<GLuint> handles;

	Textures( int size = 0 ) : handles( size ) {}

	SpecializedTexture get( int index ) {
		return SpecializedTexture( handles[ index ] );
	}

	SpecializedTexture operator []( int index ) {
		return get( index );
	}
};

template< class SpecializedTexture = Texture >
struct ScopedTextures : Textures< SpecializedTexture > {
	ScopedTextures( int size = 0 ) : Textures( size ) { 
		if( size ) {
			glGenTextures( size, &handles.front() );
		}
	}

	~ScopedTextures() {
		if( handles.size() ) {
			glDeleteTextures( handles.size(), &handles.front() );
		}
	}

	void resize( int size ) {
		if( handles.empty() ) {
			handles.resize( size );
			glGenTextures( size, &handles.front() );
		}
		else {
			throw std::logic_error( "ScopedTextures not zero!" );
		}
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

typedef Textures< Texture2D > Textures2D;

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
	/*struct BoundingBox {
		Eigen::Vector3f min, max;
	};

	struct BoundingSphere {
		Eigen::Vector3f center;
		float radius;
	};

	struct CullInfo {
		BoundingBox boundingBox;
		BoundingSphere boundingSphere;
	};*/

	std::shared_ptr<SGSScene> scene;

	GLuint subObjects_disptlayListBase;
	GLuint terrain_displayListBase;

	std::vector<GLuint> solidLists;
	std::vector<GLuint> alphaLists;
	std::vector<GLuint> terrainLists;

	ScopedTextures2D textures;

	Texture2D bakedTerrainTexture;

	ShaderCollection shaders;
	Program terrainProgram, objectProgram;

	struct Debug {
		bool showBoundingSpheres, showTerrainBoundingSpheres;
		DebugRender::CombinedCalls boundingSpheres, terrainBoundingSpheres;

		bool updateRenderLists;

		Debug() : showBoundingSpheres( false ), showTerrainBoundingSpheres( false ), updateRenderLists( true ) {}
	} debug;

	SGSSceneRenderer() : subObjects_disptlayListBase( 0 ), terrain_displayListBase( 0 ) {}

	~SGSSceneRenderer() {
		if( subObjects_disptlayListBase ) {
			glDeleteLists( subObjects_disptlayListBase, scene->subObjects.size() );
		}
		if( terrain_displayListBase ) {
			glDeleteLists( terrain_displayListBase, scene->subObjects.size() );
		}
	}

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
			Texture2D terrainTexture = textures[ scene->terrain.layers[ layerIndex ].textureIndex ];
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

		bakedTexture.bind();
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		
		bakedTexture.generateMipmap();
		bakedTexture.unbind();

		return bakedTexture.publish();
	}

	void reloadShaders() {
		while( true ) {
			shaders.shaders.clear();

			loadShaderCollection( shaders, "sgsScene.shaders" );

			terrainProgram.surfaceShader = shaders[ "terrain" ];
			terrainProgram.vertexShader = shaders[ "sgsMesh" ];
			
			objectProgram.surfaceShader = shaders[ "object" ];
			objectProgram.vertexShader = shaders[ "sgsMesh" ];

			if( terrainProgram.build( shaders ) && objectProgram.build( shaders ) ) {
				break;
			}

			__debugbreak();
		}
	}

	void processScene( const std::shared_ptr<SGSScene> &scene ) {
		this->scene = scene;

		// load textures
		int numTextures = scene->textures.size();
		textures.resize( numTextures );

		for( int textureIndex = 0 ; textureIndex < numTextures ; ++textureIndex ) {
			const auto &rawContent = scene->textures[ textureIndex ].rawContent;
			SOIL_load_OGL_texture_from_memory( &rawContent.front(), rawContent.size(), 0, textures[ textureIndex ].handle, SOIL_FLAG_DDS_LOAD_DIRECT | SOIL_FLAG_MIPMAPS | SOIL_FLAG_TEXTURE_REPEATS );
		}

		bakedTerrainTexture = bakeTerrainTexture( 32, 1 / 8.0 );

		// render everything into display lists
		subObjects_disptlayListBase = glGenLists( scene->subObjects.size() );

		terrain_displayListBase = glGenLists( scene->terrain.tiles.size() );

		terrainLists.reserve( scene->terrain.tiles.size() );
		solidLists.reserve( scene->subObjects.size() );
		alphaLists.reserve( scene->subObjects.size() );

		prerender();
	}


	// prerender everything into display lists for easy drawing later
	void prerender() {
		glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS );

		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_NORMAL_ARRAY );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );

		glPushAttrib( GL_ALL_ATTRIB_BITS );

		{
			auto &firstVertex = scene->vertices[0];
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex.uv[0] );

			for( int subObjectIndex = 0 ; subObjectIndex < scene->subObjects.size() ; ++subObjectIndex ) {
				const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

				glNewList( subObjects_disptlayListBase + subObjectIndex, GL_COMPILE );

				objectProgram.use();

				glEnable( GL_TEXTURE_2D );

				// apply the material
				const auto &material = subObject.material;
				
				if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
					textures[ material.textureIndex[0] ].bind();
				}
				else {
					Texture2D::unbind();
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

				Texture2D::unbind();
			}
		}

		// render terrain
		{
			auto &firstVertex = scene->terrain.vertices[0];

			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex.blendUV );

			for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
				const auto &tile = scene->terrain.tiles[ tileIndex ];

				glNewList( terrain_displayListBase + tileIndex , GL_COMPILE );

				glDrawElements( GL_TRIANGLES, tile.numIndices, GL_UNSIGNED_INT, &scene->terrain.indices.front() + tile.startIndex );
				glEndList();

				terrainLists.push_back( terrain_displayListBase + tileIndex );
			}
			//glDrawElements( GL_TRIANGLES, scene->terrain.indices.size(), GL_UNSIGNED_INT, &scene->terrain.indices.front() );
		}
		

		glPopAttrib();
		
		glPopClientAttrib();

		{
			debug.boundingSpheres.begin();

			for( int subObjectIndex = 0 ; subObjectIndex < scene->subObjects.size() ; ++subObjectIndex ) {
				const SGSScene::BoundingSphere &boundingSphere = scene->subObjects[subObjectIndex].boundingSphere;

				debug.boundingSpheres.setPosition( Eigen::Vector3f::Map( boundingSphere.center ) );
				glColor3f( 0.0, 1.0, 1.0 );
				debug.boundingSpheres.drawAbstractSphere( boundingSphere.radius, true, 5 );
			}
			debug.boundingSpheres.end();
		}
		{
			debug.terrainBoundingSpheres.begin();
			for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
				const SGSScene::BoundingSphere &boundingSphere = scene->terrain.tiles[tileIndex].bounding.sphere;

				debug.boundingSpheres.setPosition( Eigen::Vector3f::Map( boundingSphere.center ) );
				glColor3f( 1.0, 1.0, 0.0 );
				debug.boundingSpheres.drawAbstractSphere( boundingSphere.radius, true, 5 );
			}
			debug.terrainBoundingSpheres.end();
		}
	}

	void render( const Matrix4f &projectionView, const Eigen::Vector3f &worldViewerPosition ) {
		Eigen::FrustumPlanesMatrixf frustumPlanes = Eigen::Frustum::normalize( Eigen::projectionToFrustumPlanes * projectionView );
		
		if( debug.updateRenderLists ) {
			terrainLists.clear();
			for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
				const SGSScene::BoundingSphere &boundingSphere = scene->terrain.tiles[tileIndex].bounding.sphere;
				if( Eigen::Frustum::isInside( frustumPlanes, Eigen::Map< const Eigen::Vector3f>( boundingSphere.center ).eval(), -boundingSphere.radius ) ) {
					terrainLists.push_back( terrain_displayListBase + tileIndex );
				}
			}

			solidLists.clear();
			alphaLists.clear();
			for( int subObjectIndex = 0 ; subObjectIndex < scene->subObjects.size() ; ++subObjectIndex ) {
				const SGSScene::BoundingSphere &boundingSphere = scene->subObjects[subObjectIndex].boundingSphere;
				
				if( Eigen::Frustum::isInside( frustumPlanes, Eigen::Vector3f::Map( boundingSphere.center ).eval(), -boundingSphere.radius ) ) {
					if( scene->subObjects[subObjectIndex].material.alphaType == SGSScene::Material::AT_NONE ) {
						solidLists.push_back( subObjects_disptlayListBase + subObjectIndex );
					} 
					else {
						alphaLists.push_back( subObjects_disptlayListBase + subObjectIndex );
					}
				}
			}
		}
		
		// terrain rendering
		{
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );

			glDepthMask( GL_TRUE );

			bakedTerrainTexture.bind();
			bakedTerrainTexture.enable();
			terrainProgram.use();

			if( !terrainLists.empty() ) {
				glCallLists( terrainLists.size(), GL_UNSIGNED_INT, &terrainLists.front() );
			}
		}
		
			
		// object rendering
		if( !solidLists.empty() )
			glCallLists( solidLists.size(), GL_UNSIGNED_INT, &solidLists.front() );

		// sort alpha list
		boost::sort( alphaLists, [&, this] ( int indexA, int indexB ) {
				return ( Eigen::Vector3f::Map( scene->subObjects[indexA - subObjects_disptlayListBase ].boundingSphere.center ) - worldViewerPosition).squaredNorm() <
						( Eigen::Vector3f::Map( scene->subObjects[indexB - subObjects_disptlayListBase ].boundingSphere.center ) - worldViewerPosition).squaredNorm();
			}
			);

		if( !alphaLists.empty() )
			glCallLists( alphaLists.size(), GL_UNSIGNED_INT, &alphaLists.front() );

		// make sure this is turned on again, otherwise glClear wont work correctly...
		glDepthMask( GL_TRUE );


		// more state resets for debug rendering
		glDisable( GL_BLEND );
		glDisable( GL_ALPHA_TEST );

		glDepthMask( GL_TRUE );
		glDisable( GL_TEXTURE_2D );
		Program::useFixed();

		if( debug.showBoundingSpheres ) {
			debug.boundingSpheres.render();
		}
		if( debug.showTerrainBoundingSpheres ) {
			debug.terrainBoundingSpheres.render();
		}
	}
};
