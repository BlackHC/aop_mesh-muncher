#pragma once

#include "sgsScene.h"

#include "gl/glew.h"

#include "SOIL.h"

#include "glslPipeline.h"
#include <boost/range/algorithm/sort.hpp>
#include <debugRender.h>

#include <optix_world.h>

#include "glObjectWrappers.h"

#include <boost/unordered_map.hpp>

#include "MaxRectsBinPack.h"

#include "boost/range/algorithm/for_each.hpp"

#include <boost/timer/timer.hpp>

#include <ppl.h>
#include <algorithm>

#include "optixProgramInterface.h"

using namespace GL;

//////////////////////////////////////////////////////////////////////////
// from grid.h
// xyz -120-> yzx
template< typename Vector >
Vector permute( const Vector &v, const int *permutation ) {
	return Vector( v[permutation[0]], v[permutation[1]], v[permutation[2]] );
}

// yzx -120->xyz
template< typename Vector >
Vector permute_reverse( const Vector &w, const int *permutation ) {
	Vector v;
	for( int i = 0 ; i < 3 ; ++i ) {
		v[ permutation[i] ] = w[i];
	}
	return v;
}

inline Eigen::Matrix4f permutedToUnpermutedMatrix( const int *permutation ) {
	return (Eigen::Matrix4f() << Eigen::Vector3f::Unit( permutation[0] ), Eigen::Vector3f::Unit( permutation[1] ), Eigen::Vector3f::Unit( permutation[2] ), Eigen::Vector3f::Zero(), 0,0,0,1.0 ).finished();
}

inline Eigen::Matrix4f unpermutedToPermutedMatrix( const int *permutation ) {
	return (Eigen::Matrix4f() << Eigen::RowVector3f::Unit( permutation[0] ), 0.0,
		Eigen::RowVector3f::Unit( permutation[1] ), 0.0,
		Eigen::RowVector3f::Unit( permutation[2] ), 0.0,
		Eigen::RowVector4f::UnitW() ).finished();
}

/*
 struct BoundingBox {
	Eigen::Vector3f minCorner, maxCorner;

	BoundingBox() {
		reset();
	}

	void reset() {
		minCorner.setConstant( FLT_MAX );
		maxCorner.setConstant( FLT_MIN );
	}

	void mergePoint( const Eigen::Vector3f &point ) {
		minCorner = minCorner.cwiseMin( point );
		maxCorner = maxCorner.cwiseMin( point );
	}

	Eigen::Vector3f
};

struct OrthogonalShadowMapConstructor {
	BoundingBox bbox;


};
*/

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

	struct Optix {
		optix::Context context;
		optix::GeometryGroup scene;
		optix::Acceleration acceleration;
				
		struct MeshData {
			optix::Buffer indexBuffer, vertexBuffer;
			optix::Geometry geometry;
			optix::GeometryInstance geometryInstance;

			optix::Program intersect, boundingBox, closestHit, anyHit;
			optix::Material material;
		};
		MeshData terrain, objects;

		optix::Buffer textureInfos, textureIndices;
		optix::TextureSampler terrainTextureSampler;
		optix::TextureSampler objectTextureSampler;

		optix::Buffer outputBuffer;
		int width, height;

		ScopedTexture2D debugTexture;

		struct Programs
		{
			optix::Program miss, exception;
		} programs;
	} optix;

	std::shared_ptr<SGSScene> scene;

	//////////////////////////////////////////////////////////////////////////
#if 0
	struct MergedTextureInfo {		
		int offset[2];
		int size[2];
		int index;
	};
	// one merged texture per unique size
	ScopedTextures2D mergedTextures;
	// in textureIndex order
	std::vector<MergedTextureInfo> mergedTextureInfos;

	void mergeTextures( const ScopedTextures2D &textures ) {
		typedef std::pair<int, int> Resolution;
		// distribute by resolutions
		boost::unordered_map<Resolution, std::vector<int> > resolutions;

		for( int textureIndex = 0 ; textureIndex < textures.handles.size() ; textureIndex++ ) {
			int width, height;
			textures[ textureIndex ].getLevelParameter( 0, GL_TEXTURE_WIDTH, &width );
			textures[ textureIndex ].getLevelParameter( 0, GL_TEXTURE_HEIGHT, &height );

			resolutions[ Resolution( width, height ) ].push_back( textureIndex );
		}

		mergedTextures.resize( resolutions.size() );
		mergedTextureInfos.resize( textures.handles.size() );
		
		std::vector<Resolution> mergedResolutions;

		const int maxTextureSize = 8192;

		for( auto resolution = resolutions.cbegin() ; resolution != resolutions.cend() ; ++resolution ) {
			// + 4 because we use compressed textures
			const int adjustedWidth = resolution->first.first + 4;
			const int adjustedHeight = resolution->first.second + 4;

			const int numTextures = resolution->second.size();
			const int maxPerRow = maxTextureSize / adjustedWidth;
			const int numRows = numTextures > maxPerRow ? resolution->second.size() / maxPerRow + 1 : 1;
			const int numCols = numRows > 1 ? maxPerRow : resolution->second.size();

			mergedResolutions.push_back( Resolution( numCols * adjustedWidth, numRows * adjustedHeight ) );

			int textureSubIndex = 0; 
			for( int row = 0 ; row < numRows ; ++row ) {
				int realY = row * adjustedHeight;

				for( int col = 0 ; col < numCols ; ++col, ++textureSubIndex ) {
					if( textureSubIndex >= numTextures ) {
						break;
					}

					int realX = col * adjustedWidth;
					
					auto &info = mergedTextureInfos[ resolution->second[ textureSubIndex ] ];
					info.size[0] = resolution->first.first;
					info.size[1] = resolution->first.second;

					// 2 pixel border 
					info.offset[0] = realX + 2;
					info.offset[1] = realY + 2;
				}
			}			
		}
	}
#else
	typedef OptixProgramInterface::MergedTextureInfo MergedTextureInfo;

	// one merged texture per unique size
	ScopedTexture2D mergedTexture;
	// in textureIndex order
	std::vector<MergedTextureInfo> mergedTextureInfos;

	void mergeTextures( const ScopedTextures2D &textures ) {
		typedef std::pair<int, int> Resolution;
		// distribute by resolutions
		boost::unordered_map<Resolution, std::vector<int> > resolutions;

		std::vector<RectSize> rectSizes;
		rectSizes.reserve( textures.handles.size() );

		for( int textureIndex = 0 ; textureIndex < textures.handles.size() ; textureIndex++ ) {
			int width, height;
			textures[ textureIndex ].getLevelParameter( 0, GL_TEXTURE_WIDTH, &width );
			textures[ textureIndex ].getLevelParameter( 0, GL_TEXTURE_HEIGHT, &height );

			resolutions[ Resolution( width + 4, height + 4 ) ].push_back( textureIndex );

			RectSize rectSize = { width + 4, height + 4 };
			rectSizes.push_back( rectSize );
		}

		mergedTextureInfos.resize( textures.handles.size() );

		const int maxTextureSize = 8192;
		int mergedTextureSize = maxTextureSize;
		MaxRectsBinPack packer;

		packer.Init( mergedTextureSize, mergedTextureSize );
		std::vector<Rect> packedRects;
		packer.Insert( std::vector<RectSize>( rectSizes ), MaxRectsBinPack::RectBestShortSideFit );
		std::cout << packer.Occupancy() << "\n";

		packedRects = std::move( packer.GetRectangles() );

		int stepSize = maxTextureSize / 2;
		for( int step = 0 ; step < 4 ; ++step, stepSize /= 2 ) {
			const int testTextureSize = mergedTextureSize - stepSize;

			packer.Init( testTextureSize, testTextureSize );
			packer.Insert( std::vector<RectSize>( rectSizes ), MaxRectsBinPack::RectBestShortSideFit );
		
			if( packer.GetRectangles().size() == rectSizes.size() ) {
				std::cout << packer.Occupancy() << "\n";

				mergedTextureSize = testTextureSize;
				packedRects = std::move( packer.GetRectangles() );
			}
		}
		
		for( int i = 0 ; i < packedRects.size() ; i++ ) {
			const Rect &packedRect = packedRects[i];

			// find a fitting texture index
			auto &bin = resolutions[ Resolution( packedRect.width, packedRect.height ) ];
			int textureIndex = bin.back();
			bin.pop_back();

			auto &info = mergedTextureInfos[textureIndex];
			info.offset[0] = packedRect.x + 2;
			info.offset[1] = packedRect.y + 2;
			info.size[0] = packedRect.width - 4;
			info.size[1] = packedRect.height - 4;
		}

		// output texture
		// using uint instead of char[4]
		typedef unsigned __int32 RGBA;
		RGBA *mergedTextureData = new RGBA[ mergedTextureSize * mergedTextureSize ];
		{
			boost::timer::auto_cpu_timer timer;

			Concurrency::task_group blits;

			for( int textureIndex = 0 ; textureIndex < textures.handles.size() ; ++textureIndex ) {
				auto &info = mergedTextureInfos[textureIndex];

				// download the uncompressed texture
				RGBA *textureData = new RGBA[ info.size[0] * info.size[1] ];
				textures[ textureIndex ].getImage( 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData );
				
				blits.run( [&, info, textureData]() {
#define Wrap( x, width ) (((x) + (width)) % (width))
#define Texel( x, y ) textureData[ Wrap(x, info.size[0]) + Wrap(y, info.size[1]) * info.size[0] ]
#define MergedTexel( x, y ) mergedTextureData[ (x) + (y) * mergedTextureSize ]

					for( int y = -2 ; y < info.size[1] + 2 ; ++y ) {
						int mergedY = info.offset[1] + y;
						for( int x = -2 ; x < info.size[0] + 2 ; ++x ) {
							int mergedX = info.offset[0] + x;

							MergedTexel( mergedX, mergedY ) = Texel( x, y );
						}
					}
#undef MergedTexel
#undef Texel
#undef Wrap
					delete[] textureData;
				});
			}

			blits.wait();
		}

		// upload the texture and compress it
		mergedTexture.load( 0, GL_RGBA8, mergedTextureSize, mergedTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, mergedTextureData );
		// TODO: add helper functions for this (but not to object wrapper..?) [9/19/2012 kirschan2]
		mergedTexture.parameter( GL_TEXTURE_WRAP_S, GL_REPEAT );
		mergedTexture.parameter( GL_TEXTURE_WRAP_T, GL_REPEAT );
		mergedTexture.parameter( GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		mergedTexture.parameter( GL_TEXTURE_MIN_FILTER, GL_LINEAR );

		delete[] mergedTextureData;
	}
#endif

	//////////////////////////////////////////////////////////////////////////

	ScopedTextures2D textures;

	Texture2D bakedTerrainTexture;

	ShaderCollection shaders;
	Program terrainProgram, objectProgram, shadowMapProgram;

	struct Debug {
		bool showBoundingSpheres, showTerrainBoundingSpheres;
		DebugRender::CombinedCalls boundingSpheres, terrainBoundingSpheres;

		bool updateRenderLists;

		Debug() : showBoundingSpheres( false ), showTerrainBoundingSpheres( false ), updateRenderLists( true ) {}
	} debug;

	struct MultiDrawList {
		std::vector< GLsizei > counts;
		std::vector< const void * > indices;

		bool empty() const {
			return counts.empty();
		}

		void reserve( size_t capacity ) {
			counts.reserve( capacity );
			indices.reserve( capacity );
		}

		void push_back( GLsizei count, const void *firstIndex ) {
			counts.push_back( count );
			indices.push_back( firstIndex );
		}

		size_t size() const {
			return counts.size();
		}
	};

	std::vector<int> solidLists;
	std::vector<int> alphaLists;
	std::vector<int> terrainLists;

	GL::ScopedBuffer objectVertices, objectIndices, terrainVertices, terrainIndices;
	GL::ScopedVertexArrayObject objectVAO, terrainVAO;
	// one display list per sub object
	GL::ScopedDisplayLists materialDisplayLists;

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

		ScopedFramebufferObject fbo;
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

			shadowMapProgram.vertexShader = shaders[ "shadowMapMesh" ];
			shadowMapProgram.surfaceShader = shaders[ "shadowMapPixelShader" ];

			if( terrainProgram.build( shaders ) && objectProgram.build( shaders ) && shadowMapProgram.build( shaders ) ) {
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

			int width, height;
			textures[ textureIndex ].getLevelParameter( 0, GL_TEXTURE_WIDTH, &width );
			textures[ textureIndex ].getLevelParameter( 0, GL_TEXTURE_HEIGHT, &height );

			std::cout << width << "x" << height << "\n";
		}

		bakedTerrainTexture = bakeTerrainTexture( 32, 1 / 8.0 );

		// render everything into display lists
		terrainLists.reserve( scene->terrain.tiles.size() );
		solidLists.reserve( scene->subObjects.size() );
		alphaLists.reserve( scene->subObjects.size() );

		loadBuffers();
		setVertexArrayObjects();

		prepareMaterialDisplayLists();

		prerender();

		mergeTextures( textures );
	}

	void loadBuffers() {
		objectVertices.bufferData( scene->vertices.size() * sizeof( SGSScene::Vertex ), &scene->vertices.front(), GL_STATIC_DRAW );
		objectIndices.bufferData( scene->indices.size() * sizeof( unsigned ), &scene->indices.front(), GL_STATIC_DRAW );

		terrainVertices.bufferData( scene->terrain.vertices.size() * sizeof( SGSScene::Terrain::Vertex ), &scene->terrain.vertices.front(), GL_STATIC_DRAW );
		terrainIndices.bufferData( scene->terrain.indices.size() * sizeof( unsigned ), &scene->terrain.indices.front(), GL_STATIC_DRAW );
	}

	void setVertexArrayObjects() {
		// object vertex array object
		{
			objectVAO.bind();
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );

			objectVertices.bind( GL_ARRAY_BUFFER );
			SGSScene::Vertex *firstVertex = nullptr;
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex->position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex->normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex->uv[0] );

			objectIndices.bind( GL_ELEMENT_ARRAY_BUFFER );
			objectVAO.unbind();
		}
		// terrain vertex array object
		{
			terrainVAO.bind();
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );

			terrainVertices.bind( GL_ARRAY_BUFFER );
			SGSScene::Terrain::Vertex *firstVertex = nullptr;
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex->position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex->normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex->blendUV );

			terrainIndices.bind( GL_ELEMENT_ARRAY_BUFFER );
			terrainVAO.unbind();
		}

		GL::Buffer::unbind( GL_ARRAY_BUFFER );
		GL::Buffer::unbind( GL_ELEMENT_ARRAY_BUFFER );

		glDisableClientState( GL_VERTEX_ARRAY );
		glDisableClientState( GL_NORMAL_ARRAY );
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	void prepareMaterialDisplayLists() {
		materialDisplayLists.resize( scene->subObjects.size() );

		for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
			const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

			materialDisplayLists[ subObjectIndex ].begin();

			// apply the material
			const auto &material = subObject.material;

			if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
				textures[ material.textureIndex[0] ].bind();
				Texture2D::enable();
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

			/*if( material.doubleSided ) {
				glDisable( GL_CULL_FACE );
			}
			else {
				glEnable( GL_CULL_FACE );
			}*/

			glColor4ub( material.diffuse.r, material.diffuse.g, material.diffuse.b, alpha );

			GL::DisplayList::end();
		}
		Texture2D::unbind();
	}


	// prerender everything into display lists for easy drawing later
	void prerender() {
		{
			debug.boundingSpheres.begin();

			for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
				const SGSScene::BoundingSphere &boundingSphere = scene->subObjects[subObjectIndex].bounding.sphere;

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

	ScopedTexture2D sunShadowMap;
	Eigen::Matrix4f sunProjectionMatrix;
	int sunShadowMapSize;

	void initShadowMap() {
		sunShadowMapSize = 8096;
		sunShadowMap.load( 0, GL_DEPTH_COMPONENT32F, sunShadowMapSize, sunShadowMapSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr );
		sunShadowMap.parameter( GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
		sunShadowMap.parameter( GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );

		sunShadowMap.parameter( GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE );
		sunShadowMap.parameter( GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
		sunShadowMap.parameter( GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
		sunShadowMap.parameter( GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		sunShadowMap.parameter( GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	}

	void initShadowMapProjectionMatrix( const Eigen::AlignedBox3f &boundingBox, const Eigen::Vector3f &direction ) {
#if 0
		// determine the main direction
		int mainAxis;
		{
			auto weightedDirection = direction.cwiseProduct( boundingBox.sizes() );
			weightedDirection.cwiseAbs().maxCoeff( &mainAxis );
		}

		int permutation[3] = { (mainAxis + 1) % 3, (mainAxis + 2) % 3, mainAxis };
		auto permutedDirection = permute( direction, permutation );

		Eigen::AlignedBox3f permutedBox;
		permutedBox.extend( permute( boundingBox.min(), permutation ) );
		permutedBox.extend( permute( boundingBox.max(), permutation ) );

		/*sunProjectionMatrix = Eigen::createShearProjectionMatrixLH( )
			Eigen::createOrthoProjectionMatrixLH( permutedBox.min(), permutedBox.max() ) * unpermutedToPermutedMatrix( permutation );*/
		sunProjectionMatrix = Eigen::createOrthoProjectionMatrixLH( permutedBox.min(), permutedBox.max() ) * unpermutedToPermutedMatrix( permutation );
#else
		using namespace Eigen;

		Matrix4f lightRotation = Eigen::createViewerMatrixLH( Vector3f::Zero(), direction, Vector3f::UnitX() );
		AlignedBox3f lightVolumeBox;
		for( int cornerIndex = 0 ; cornerIndex < 8 ; ++cornerIndex ) {
			lightVolumeBox.extend( (lightRotation * boundingBox.corner( AlignedBox3f::CornerType( cornerIndex ) ).homogeneous().eval()).hnormalized() );
		}

		sunProjectionMatrix = Eigen::createOrthoProjectionMatrixLH( lightVolumeBox.min(), lightVolumeBox.max() ) * lightRotation;
#endif
	}

	void renderShadowmap() {
		using namespace Eigen;
		initShadowMap();

		// calculate bounding box
		AlignedBox3f sceneBoundingBox;
		for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
			const SGSScene::BoundingBox &boundingBox = scene->terrain.tiles[tileIndex].bounding.box;

			sceneBoundingBox.extend( AlignedBox3f( Vector3f::Map( boundingBox.min ), Vector3f::Map( boundingBox.max ) ) );
		}

		for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
			const SGSScene::BoundingBox &boundingBox = scene->subObjects[subObjectIndex].bounding.box;

			sceneBoundingBox.extend( AlignedBox3f( Vector3f::Map( boundingBox.min ), Vector3f::Map( boundingBox.max ) ) );
		}

		initShadowMapProjectionMatrix( sceneBoundingBox, Vector3f( 0.0, -1.0, -1.0 ).normalized() );

		ScopedFramebufferObject fbo;
		fbo.attach( sunShadowMap, GL_DEPTH_ATTACHMENT, 0 );

		fbo.bind();
		fbo.setDrawBuffers();

		glDisable( GL_CULL_FACE );
		glCullFace( GL_FRONT );

		glPushAttrib( GL_VIEWPORT_BIT );
		glViewport( 0, 0, sunShadowMapSize, sunShadowMapSize );

		glClear( GL_DEPTH_BUFFER_BIT );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

		glMatrixMode( GL_PROJECTION );
		glLoadMatrix( sunProjectionMatrix );

		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();

		shadowMapProgram.use();		

		buildDrawLists( sunProjectionMatrix );

		// draw terrain
		{
			terrainVAO.bind();
			glDrawElements( GL_TRIANGLES, scene->terrain.indices.size(), GL_UNSIGNED_INT, nullptr );
			terrainVAO.unbind();
		}
			
		{
			objectVAO.bind();

			GLuint *firstIndex = nullptr;
			for( int i = 0 ; i < solidLists.size() ; i++ ) {
				const int subObjectIndex = solidLists[i];
				glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
			}

			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GREATER, 0.5 );

			for( int i = 0 ; i < alphaLists.size() ; i++ ) {
				const int subObjectIndex = alphaLists[i];
				const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

				const auto &material = subObject.material;
				if( material.alphaType == SGSScene::Material::AT_ADDITIVE ) {
					continue;
				}
				if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
					textures[ material.textureIndex[0] ].bind();
					Texture2D::enable();
				}
				else {
					Texture2D::unbind();
				}

				glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
			}

			objectVAO.unbind();
		}

		// state reset
		{
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glDisable( GL_CULL_FACE );

			glDepthMask( GL_TRUE );
			glDisable( GL_TEXTURE_2D );
		}

		// reset state
		fbo.unbind();
		fbo.resetDrawBuffers();

		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glPopAttrib();

		//glStringMarkerGREMEDY( 0, "shadow map done" );
	}

	void buildDrawLists( const Matrix4f &projectionView ) {
		Eigen::FrustumPlanesMatrixf frustumPlanes = Eigen::Frustum::normalize( Eigen::projectionToFrustumPlanes * projectionView );

		if( debug.updateRenderLists ) {
			terrainLists.clear();
			for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
				const SGSScene::BoundingSphere &boundingSphere = scene->terrain.tiles[tileIndex].bounding.sphere;
				if( Eigen::Frustum::isInside( frustumPlanes, Eigen::Map< const Eigen::Vector3f>( boundingSphere.center ).eval(), -boundingSphere.radius ) ) {
					terrainLists.push_back( tileIndex );
				}
			}

			solidLists.clear();
			alphaLists.clear();
			for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
				const SGSScene::BoundingSphere &boundingSphere = scene->subObjects[subObjectIndex].bounding.sphere;

				if( Eigen::Frustum::isInside( frustumPlanes, Eigen::Vector3f::Map( boundingSphere.center ).eval(), -boundingSphere.radius ) ) {
					if( scene->subObjects[subObjectIndex].material.alphaType == SGSScene::Material::AT_NONE ) {
						solidLists.push_back( subObjectIndex );
					}
					else {
						alphaLists.push_back( subObjectIndex );
					}
				}
			}
		}
	}

	void sortAlphaList( const Eigen::Vector3f &worldViewerPosition ) {
		boost::sort(
			alphaLists, [&, this] ( int indexA, int indexB ) {
				return ( Eigen::Vector3f::Map( scene->subObjects[indexA].bounding.sphere.center ) - worldViewerPosition).squaredNorm() >
					( Eigen::Vector3f::Map( scene->subObjects[indexB].bounding.sphere.center ) - worldViewerPosition).squaredNorm();
		}
		);
	}

	void render( const Matrix4f &projectionView, const Eigen::Vector3f &worldViewerPosition ) {
		buildDrawLists( projectionView );
		sortAlphaList( worldViewerPosition );

		glDisable( GL_CULL_FACE );
		glCullFace( GL_BACK );

		// set the shadow map
		glActiveTexture( GL_TEXTURE1 );
		sunShadowMap.bind();
		glActiveTexture( GL_TEXTURE0 );

		// terrain rendering
		{
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );

			glDepthMask( GL_TRUE );

			bakedTerrainTexture.bind();
			bakedTerrainTexture.enable();
			terrainProgram.use();

			// hack
			glUniform( terrainProgram.uniformLocations[ "viewerPosition" ], worldViewerPosition );
			glUniform( terrainProgram.uniformLocations[ "sunShadowProjection" ], sunProjectionMatrix );

			MultiDrawList drawList;
			drawList.reserve( terrainLists.size() );			
			GLuint *firstIndex = nullptr;
			for( int i = 0 ; i < terrainLists.size() ; i++ ) {
				const int tileIndex = terrainLists[i];
				drawList.push_back( scene->terrain.tiles[tileIndex].numIndices, firstIndex + scene->terrain.tiles[tileIndex].startIndex );
			}

			if( !drawList.empty() ) {
				terrainVAO.bind();
				glMultiDrawElements( GL_TRIANGLES, &drawList.counts.front(), GL_UNSIGNED_INT, &drawList.indices.front(), drawList.size() );
				terrainVAO.unbind();
			}
		}

		// object rendering
		{
			objectProgram.use();

			// hack
			glUniform( objectProgram.uniformLocations[ "viewerPosition" ], worldViewerPosition );
			glUniform( objectProgram.uniformLocations[ "sunShadowProjection" ], sunProjectionMatrix );

			objectVAO.bind();

			GLuint *firstIndex = nullptr;
			for( int i = 0 ; i < solidLists.size() ; i++ ) {
				const int subObjectIndex = solidLists[i];
				materialDisplayLists[ subObjectIndex ].call();
				glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
			}

			for( int i = 0 ; i < alphaLists.size() ; i++ ) {
				const int subObjectIndex = alphaLists[i];
				materialDisplayLists[ subObjectIndex ].call();
				glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
			}

			objectVAO.unbind();
		}

		// make sure this is turned on again, otherwise glClear wont work correctly...
		glDepthMask( GL_TRUE );
		
		{
			// more state resets for debug rendering
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glDisable( GL_CULL_FACE );

			glDepthMask( GL_TRUE );
			glDisable( GL_TEXTURE_2D );
			Program::useFixed();

			if( debug.showBoundingSpheres ) {
				debug.boundingSpheres.render();
			}
			if( debug.showTerrainBoundingSpheres ) {
				debug.terrainBoundingSpheres.render();
			}

			DebugRender::ImmediateCalls lightFrustum;
			lightFrustum.begin();
			glMatrixMode( GL_MODELVIEW );
			glMultMatrix( sunProjectionMatrix.inverse() );
			lightFrustum.drawBox( Eigen::Vector3f::Constant( 2.0 ), true, true );
			lightFrustum.end();
		}
	}

	void initOptix() {
		optix.context = optix::Context::create();

		optix.context->setRayTypeCount(2);
		optix.context->setEntryPointCount(1);
		optix.context->setStackSize(8000);
		optix.context->setPrintBufferSize(65536);
		optix.context->setPrintEnabled(true);

		const char *raytracerPtxFilename = "cuda_compile_ptx_generated_raytracer.cu.ptx";		
		optix.programs.exception = optix.context->createProgramFromPTXFile (raytracerPtxFilename, "exception");
		optix.programs.miss = optix.context->createProgramFromPTXFile (raytracerPtxFilename, "miss");

		const char *terrainMeshPtxFilename = "cuda_compile_ptx_generated_terrainMesh.cu.ptx";
		optix.terrain.intersect = optix.context->createProgramFromPTXFile (terrainMeshPtxFilename, "intersect");
		optix.terrain.boundingBox = optix.context->createProgramFromPTXFile (terrainMeshPtxFilename, "bounding_box");
		optix.terrain.anyHit = optix.context->createProgramFromPTXFile (terrainMeshPtxFilename, "any_hit");
		optix.terrain.closestHit = optix.context->createProgramFromPTXFile (terrainMeshPtxFilename, "closest_hit");

		const char *objectMeshPtxFilename = "cuda_compile_ptx_generated_objectMesh.cu.ptx";
		optix.objects.intersect = optix.context->createProgramFromPTXFile (objectMeshPtxFilename, "intersect");
		optix.objects.boundingBox = optix.context->createProgramFromPTXFile (objectMeshPtxFilename, "bounding_box");
		optix.objects.anyHit = optix.context->createProgramFromPTXFile (objectMeshPtxFilename, "any_hit");
		optix.objects.closestHit = optix.context->createProgramFromPTXFile (objectMeshPtxFilename, "closest_hit");

		optix.context->setRayGenerationProgram (0, optix.context->createProgramFromPTXFile (raytracerPtxFilename, "ray_gen"));

		// setup the objects buffers
		{
			optix.objects.material = optix.context->createMaterial ();
			optix.objects.material->setAnyHitProgram (1, optix.objects.anyHit);
			optix.objects.material->setClosestHitProgram (0, optix.objects.closestHit);
			optix.objects.material->validate ();

			int primitiveCount = scene->numSceneIndices / 3;

			optix.objects.indexBuffer = optix.context->createBufferFromGLBO( RT_BUFFER_INPUT, objectIndices.handle );
			optix.objects.indexBuffer->setFormat( RT_FORMAT_UNSIGNED_INT3 );
			optix.objects.indexBuffer->setSize( primitiveCount );
			optix.objects.indexBuffer->validate();

			optix.objects.vertexBuffer = optix.context->createBufferFromGLBO( RT_BUFFER_INPUT, objectVertices.handle );
			optix.objects.vertexBuffer->setSize( scene->numSceneVertices );
			optix.objects.vertexBuffer->setFormat( RT_FORMAT_USER );
			optix.objects.vertexBuffer->setElementSize( sizeof SGSScene::Vertex );
			optix.objects.vertexBuffer->validate();

			optix.objects.geometry = optix.context->createGeometry ();
			optix.objects.geometry ["vertexBuffer"]->setBuffer (optix.objects.vertexBuffer);
			optix.objects.geometry ["indexBuffer"]->setBuffer (optix.objects.indexBuffer);
			optix.objects.geometry->setBoundingBoxProgram (optix.objects.boundingBox);
			optix.objects.geometry->setIntersectionProgram (optix.objects.intersect);

			optix.objects.geometry->setPrimitiveCount ( primitiveCount );
			optix.objects.geometry->validate ();

			optix.objects.geometryInstance = optix.context->createGeometryInstance (optix.objects.geometry, &optix.objects.material, &optix.objects.material + 1);
			optix.objects.geometryInstance->validate ();

			optix.objectTextureSampler = optix.context->createTextureSamplerFromGLImage( mergedTexture.handle, RT_TARGET_GL_TEXTURE_2D );
			optix.objectTextureSampler->setWrapMode( 0, RT_WRAP_REPEAT );
			optix.objectTextureSampler->setWrapMode( 1, RT_WRAP_REPEAT );
			// we can still use tex2d here and linear interpolation with array indexing [9/19/2012 kirschan2]
			optix.objectTextureSampler->setIndexingMode( RT_TEXTURE_INDEX_ARRAY_INDEX );
			optix.objectTextureSampler->setReadMode( RT_TEXTURE_READ_NORMALIZED_FLOAT );
			optix.objectTextureSampler->setMaxAnisotropy( 1.0f );
			optix.objectTextureSampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

			optix.objects.material[ "objectTexture" ]->setTextureSampler( optix.objectTextureSampler );

			optix.textureInfos = optix.context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, mergedTextureInfos.size() );
			optix.textureInfos->setElementSize( sizeof MergedTextureInfo );
			::memcpy( optix.textureInfos->map(), &mergedTextureInfos.front(), sizeof( MergedTextureInfo ) * mergedTextureInfos.size() );
			optix.textureInfos->unmap();
			optix.textureInfos->validate();

			// set texture indices
			optix.textureIndices = optix.context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT, primitiveCount );

			int *textureIndices = (int *) optix.textureIndices->map();

			for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
				const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

				const int textureIndex = subObject.material.textureIndex[0];

				const int startPrimitive = subObject.startIndex / 3;
				const int endPrimitive = startPrimitive + subObject.numIndices / 3;
				std::fill( textureIndices + startPrimitive, textureIndices + endPrimitive, textureIndex );
			}
			optix.textureIndices->unmap();
			optix.textureIndices->validate();

			optix.context[ "textureIndices" ]->setBuffer( optix.textureIndices );
			optix.objects.material[ "textureInfos" ]->setBuffer( optix.textureInfos );
		}

		// set up the terrain buffers
		{
			optix.terrain.material = optix.context->createMaterial ();
			optix.terrain.material->setClosestHitProgram (0, optix.terrain.closestHit);
			optix.terrain.material->setAnyHitProgram (1, optix.terrain.anyHit);
			optix.terrain.material->validate ();

			int primitiveCount = scene->terrain.indices.size() / 3;
			optix.terrain.indexBuffer = optix.context->createBufferFromGLBO( RT_BUFFER_INPUT, terrainIndices.handle );
			optix.terrain.indexBuffer->setFormat( RT_FORMAT_UNSIGNED_INT3 );
			optix.terrain.indexBuffer->setSize( primitiveCount );
			optix.terrain.indexBuffer->validate();

			optix.terrain.vertexBuffer = optix.context->createBufferFromGLBO( RT_BUFFER_INPUT, terrainVertices.handle );
			optix.terrain.vertexBuffer->setSize( scene->terrain.vertices.size() );
			optix.terrain.vertexBuffer->setFormat( RT_FORMAT_USER );
			optix.terrain.vertexBuffer->setElementSize( sizeof SGSScene::Terrain::Vertex );
			optix.terrain.vertexBuffer->validate();

			optix.terrain.geometry = optix.context->createGeometry ();
			optix.terrain.geometry ["vertexBuffer"]->setBuffer (optix.terrain.vertexBuffer);
			optix.terrain.geometry ["indexBuffer"]->setBuffer (optix.terrain.indexBuffer);
			optix.terrain.geometry->setBoundingBoxProgram (optix.terrain.boundingBox);
			optix.terrain.geometry->setIntersectionProgram (optix.terrain.intersect);

			optix.terrain.geometry->setPrimitiveCount ( primitiveCount );
			optix.terrain.geometry->validate ();

			optix.terrainTextureSampler = optix.context->createTextureSamplerFromGLImage( bakedTerrainTexture.handle, RT_TARGET_GL_TEXTURE_2D );
			optix.terrainTextureSampler->setWrapMode( 0, RT_WRAP_REPEAT );
			optix.terrainTextureSampler->setWrapMode( 1, RT_WRAP_REPEAT );
			optix.terrainTextureSampler->setIndexingMode( RT_TEXTURE_INDEX_NORMALIZED_COORDINATES );
			optix.terrainTextureSampler->setReadMode( RT_TEXTURE_READ_NORMALIZED_FLOAT );
			optix.terrainTextureSampler->setMaxAnisotropy( 1.0f );
			optix.terrainTextureSampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

			optix.terrain.material[ "terrainTexture" ]->setTextureSampler( optix.terrainTextureSampler );

			optix.terrain.geometryInstance = optix.context->createGeometryInstance (optix.terrain.geometry, &optix.terrain.material, &optix.terrain.material + 1);
			optix.terrain.geometryInstance->validate ();
		}

		optix.acceleration = optix.context->createAcceleration ("Bvh", "Bvh");

		// make dirty is not really needed here, is it?
		optix.acceleration->markDirty ();
		optix.acceleration->validate ();

		optix.context->setMissProgram (0, optix.programs.miss);
		optix.context->setExceptionProgram (0, optix.programs.exception);
		optix.context->validate ();

		optix.scene = optix.context->createGeometryGroup ();
		optix.scene->setAcceleration (optix.acceleration);
		optix.scene->setChildCount (2);
		optix.scene->setChild (0, optix.objects.geometryInstance);
		optix.scene->setChild (1, optix.terrain.geometryInstance);
		optix.scene->validate ();

		optix.outputBuffer = optix.context->createBuffer( RT_BUFFER_OUTPUT,  RT_FORMAT_UNSIGNED_BYTE4, optix.width = 640, optix.height = 480 );

		optix.context ["rootObject"]->set (optix.scene);

		optix.context ["resultBuffer"]->setBuffer (optix.outputBuffer);

		auto r = rtContextCompile (optix.context->get());
		const char* e;
		rtContextGetErrorString (optix.context->get(), r, &e);
		std::cout << e;
	}

	void renderOptix(const Matrix4f &projectionView, const Eigen::Vector3f &worldViewerPosition) {
		optix.context[ "eyePosition" ]->set3fv( worldViewerPosition.data() );
		
		// this works with all usual projection matrices (where x and y don't have any effect on z and w in clip space)
		// determine u, v, and w by unprojecting (x,y,-1,1) from clip space to world spacel
		Eigen::Matrix4f inverseProjectionView = projectionView.inverse();
		// this is the w coordinate of the unprojected coordinates
		const float unprojectedW = inverseProjectionView(3,3) - inverseProjectionView(3,2);

		// divide the homogeneous affine matrix by the projected w
		// see R1 page for deduction
		Eigen::Matrix< float, 3, 4> inverseProjectionView34 = projectionView.inverse().topLeftCorner<3,4>() / unprojectedW;
		const Eigen::Vector3f u = inverseProjectionView34.col(0);
		const Eigen::Vector3f v = inverseProjectionView34.col(1);
		const Eigen::Vector3f w = inverseProjectionView34.col(3) - inverseProjectionView34.col(2) - worldViewerPosition;

		optix.context[ "U" ]->set3fv( u.data() );
		optix.context[ "V" ]->set3fv( v.data() );
		optix.context[ "W" ]->set3fv( w.data() );

		optix.context->launch(0, optix.width, optix.height );

		void *data = optix.outputBuffer->map();
		optix.debugTexture.load( 0, GL_RGBA8, optix.width, optix.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		//SOIL_save_image( "frame.bmp", SOIL_SAVE_TYPE_BMP, 640, 480, 4, (unsigned char*) data );
		optix.outputBuffer->unmap();
	}
};
