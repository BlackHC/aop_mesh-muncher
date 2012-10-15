#pragma once

#include <gl/glew.h>
#include <SOIL.h>

#include "glObjectWrappers.h"
#include <debugRender.h>
#include "glslPipeline.h"

#include "sgsScene.h"

#include "optixProgramInterface.h"
SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::MergedTextureInfo );

#include <Eigen/Eigen>

#include "eigenProjectionMatrices.h"

#include "boost/range/algorithm/copy.hpp"

#include <contextHelper.h>
#include <vector>
#include "rendering.h"

#include <unsupported/Eigen/openglsupport>
//#include "boost/multi_array.hpp"

#include <gridStorage.h>

namespace Eigen {
	// DSA support
	EIGEN_GL_FUNC1_DECLARATION       (glMatrixLoad,GLenum,const)
	EIGEN_GL_FUNC1_SPECIALIZATION_MAT(glMatrixLoad,GLenum,const,float,  4,4,fEXT)
	EIGEN_GL_FUNC1_SPECIALIZATION_MAT(glMatrixLoad,GLenum,const,double, 4,4,dEXT)
}


using namespace GL;

//////////////////////////////////////////////////////////////////////////
struct OptixRenderer;

struct Instance {
	// object to world
	Eigen::Affine3f transformation;

	int modelId;
};

namespace VoxelizedModel {
		struct NormalOverdraw4ub {
			// -1..+1 packed into 0..255
			unsigned char nx, ny, nz, numSamples;
		};
		
		//typedef boost::multi_array< Color4ub, 3 > Voxels;
		typedef GridStorage<NormalOverdraw4ub> Voxels;
};

struct SGSSceneRenderer {
	struct Optix {
		optix::GeometryGroup staticScene, dynamicScene;
		optix::Acceleration staticAcceleration, dynamicAcceleration;
				
		struct MeshData {
			optix::Buffer indexBuffer, vertexBuffer;
			optix::Geometry geometry;
			optix::GeometryInstance geometryInstance;
		};

		struct ObjectMeshData : MeshData {
			optix::Buffer materialIndices, materialInfos;
		};

		MeshData terrain;
		ObjectMeshData staticObjects, dynamicObjects;

		optix::Material terrainMaterial, objectMaterial;

		optix::Buffer textureInfos;
		optix::TextureSampler terrainTextureSampler;
		optix::TextureSampler objectTextureSampler;

		struct Cache {
			static const int VERSION = 1;

			int magicStamp;
			std::vector<unsigned char> staticSceneAccelerationCache;
			std::vector<std::vector<unsigned char>> prototypesAccelerationCache;

			SERIALIZER_DEFAULT_IMPL( (magicStamp)(staticSceneAccelerationCache)(prototypesAccelerationCache) );
		};
	} optix;

	std::shared_ptr<SGSScene> scene;

	//////////////////////////////////////////////////////////////////////////
	typedef OptixProgramInterface::MergedTextureInfo MergedTextureInfo;

	// one merged texture per unique size
	ScopedTexture2D mergedTexture;
	// in textureIndex order
	std::vector<MergedTextureInfo> mergedTextureInfos;

	void mergeTextures( const ScopedTextures2D &textures );

	//////////////////////////////////////////////////////////////////////////

	ScopedTextures2D textures;

	ScopedTexture2D bakedTerrainTexture;

	ShaderCollection shaders;
	Program terrainProgram, objectProgram, previewObjectProgram, shadowMapProgram;

	// voxelizer shaders
	ShaderCollection voxelizerShaders;
	Program voxelizerSplatterProgram, voxelizerMuxerProgram;

	struct Debug {
		bool showBoundingSpheres, showTerrainBoundingSpheres;
		DebugRender::CombinedCalls boundingSpheres, terrainBoundingSpheres;

		bool updateRenderLists;

		Debug() : showBoundingSpheres( false ), showTerrainBoundingSpheres( false ), updateRenderLists( true ) {}
	} debug;

	std::vector<int> solidLists;
	std::vector<int> alphaLists;
	std::vector<int> terrainLists;

	struct Mesh {
		GL::ScopedBuffer vertexBuffer, indexBuffer;
		GL::ScopedVertexArrayObject vao;
	};

	struct ObjectMesh : Mesh {
		void init() {
			vao.bind();
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );

			vertexBuffer.bind( GL_ARRAY_BUFFER );
			SGSScene::Vertex *firstVertex = nullptr;
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex->position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex->normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Vertex ), firstVertex->uv[0] );

			indexBuffer.bind( GL_ELEMENT_ARRAY_BUFFER );
			vao.unbind();
		}
	};

	struct TerrainMesh : Mesh {
		void init() {
			vao.bind();
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );

			vertexBuffer.bind( GL_ARRAY_BUFFER );
			SGSScene::Terrain::Vertex *firstVertex = nullptr;
			glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex->position );
			glNormalPointer( GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex->normal );
			glTexCoordPointer( 2, GL_FLOAT, sizeof( SGSScene::Terrain::Vertex ), firstVertex->blendUV );

			indexBuffer.bind( GL_ELEMENT_ARRAY_BUFFER );
			vao.unbind();
		}
	};

	ObjectMesh staticObjectsMesh;
	TerrainMesh terrainMesh;
	
	// one display list per sub object
	GL::ScopedDisplayLists materialDisplayLists;

	struct Cache {
		static const int VERSION = 1;

		struct TextureDump {
			int width;
			int height;

			std::vector<unsigned char> image;

			SERIALIZER_DEFAULT_IMPL( (width)(height)(image) );

			void dump( const Texture2D &texture ) {
				texture.getLevelParameter( 0, GL_TEXTURE_WIDTH, &width );
				texture.getLevelParameter( 0, GL_TEXTURE_HEIGHT, &height );

				int imageSize = width * height * 4;

				image.resize( imageSize );
				texture.getImage( 0, GL_RGBA, GL_UNSIGNED_BYTE, &image.front() );
			}

			void load( Texture2D &texture ) {
				texture.load( 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image.front() );
			}
		};
		
		int magicStamp;

		TextureDump bakedTerrainTexture;
		TextureDump mergedObjectTextures;
		std::vector<MergedTextureInfo> mergedTextureInfos;

		SERIALIZER_DEFAULT_IMPL( (magicStamp)(bakedTerrainTexture)(mergedTextureInfos)(mergedObjectTextures) );
	};
		
	SGSSceneRenderer() {}

	// TODO> move this into scene [10/13/2012 kirschan2]
	unsigned getSceneHash() {
		int xor1 = scene->numSceneIndices ^ scene->numSceneVertices;
		int xor2 = scene->numSceneObjects ^ scene->numSceneSubObjects;
		int xor3 = scene->textures.size() ^ scene->terrain.mapSize[0] ^ scene->terrain.mapSize[1];
		return (xor1 << 16) | (xor1 >> 16) | xor2 | (xor3 << 8);
	}

	void bakeTerrainTexture( int detailFactor, float textureDetailFactor );

	void reloadShaders();

	void processScene( const std::shared_ptr<SGSScene> &scene, const char *cacheFilename );
	void loadStaticBuffers();
	void setVertexArrayObjects();
	void prepareMaterialDisplayLists();

	// prerender everything into display lists for easy drawing later
	void prerenderDebugInfos();

	ScopedTexture2D sunShadowMap;
	Eigen::Matrix4f sunProjectionMatrix;
	int sunShadowMapSize;

	Eigen::AlignedBox3f sceneBoundingBox;

	void updateSceneBoundingBox();

	void initShadowMap();
	void initShadowMapProjectionMatrix( const Eigen::AlignedBox3f &boundingBox, const Eigen::Vector3f &direction );
	void renderShadowmap( const RenderContext &renderContext );

	void buildDrawLists( const Eigen::Matrix4f &projectionView, const RenderContext &renderContext );
	void sortAlphaList( const Eigen::Vector3f &worldViewerPosition );

	void drawModel( SGSScene::Model &model );
	void drawInstance( Instance &instance );

	void resetState();
	void render( const Eigen::Matrix4f &projectionView, const Eigen::Vector3f &worldViewerPosition, const RenderContext &renderContext );

	// renders a model at the origin
	void renderModel( const Eigen::Vector3f &worldViewerPosition, int modelIndex );

	VoxelizedModel::Voxels voxelizeModel( int modelIndex, float resolution );

	//////////////////////////////////////////////////////////////////////////
	// TOOD: move optix specific code into its own class [9/30/2012 kirschan2]
	void initOptix( OptixRenderer *optixRenderer );
	bool loadOptixCache();
	void writeOptixCache();
	void refillDynamicOptixBuffers();
	//////////////////////////////////////////////////////////////////////////

	// TODO: most of this should be moved into ModelDatabase [10/13/2012 kirschan2]
	std::vector< Instance > instances;

	int getNumInstances() const {
		return int( scene->numSceneObjects + instances.size() );
	}

	bool isDynamicInstance( int instanceIndex ) {
		return instanceIndex >= scene->numSceneObjects;
	}

	int addInstance( const Instance &instance );

	void removeInstance( int instanceIndex );

	void setInstanceTransformation( int instanceIndex, const Eigen::Affine3f &transformation ) {
		instances[ instanceIndex - scene->numSceneObjects ].transformation = transformation;

		refillDynamicOptixBuffers();
	}

	Eigen::Affine3f getInstanceTransformation( int instanceIndex ) const {
		if( instanceIndex < scene->objects.size() ) {
			return Eigen::Affine3f( Eigen::Matrix4f::Map( scene->objects[ instanceIndex ].transformation ) );
		}
		else {
			instanceIndex -= int( scene->objects.size() );
			return instances[ instanceIndex ].transformation;
		}
	}

	Eigen::AlignedBox3f getBoundingBox( const SGSScene::Model &model ) const {
		return Eigen::AlignedBox3f(
			Eigen::Vector3f::Map( model.bounding.box.min ),
			Eigen::Vector3f::Map( model.bounding.box.max )
		);
	}

	Eigen::AlignedBox3f getUntransformedInstanceBoundingBox( int instanceIndex ) const {
		return getBoundingBox( getInstanceModel( instanceIndex ) );
	}

	struct BoundingSphere {
		Eigen::Vector3f center;
		float radius;

		BoundingSphere( const Eigen::Vector3f &center, float radius ) :
			center( center ),
			radius( radius )
		{
		}
	};

	BoundingSphere getBoundingSphere( const SGSScene::Model &model ) const {
		return BoundingSphere(
			Eigen::Vector3f::Map( model.bounding.sphere.center ),
			model.bounding.sphere.radius
		);
	}

	BoundingSphere getUntransformedInstanceBoundingSphere( int instanceIndex ) const {
		return getBoundingSphere( getModel( getModelIndex( instanceIndex ) ) );
	}

	Eigen::AlignedBox3f getModelBoundingBox( int modelIndex ) const {
		return getBoundingBox( getModel( modelIndex ) );
	}

	int getModelIndex( int instanceIndex ) const {
		if( instanceIndex < scene->objects.size() ) {
			return scene->objects[ instanceIndex ].modelId;
		}
		else {
			instanceIndex -= int( scene->objects.size() );
			return instances[ instanceIndex ].modelId;
		}
	}

	const SGSScene::Model &getModel( int modelIndex ) const {
		return scene->models[ modelIndex ];
	}

	const SGSScene::Model &getInstanceModel( int instanceIndex ) const {
		return scene->models[ getModelIndex( instanceIndex ) ];
	}

	typedef std::vector<int> InstanceIndices;
	
	InstanceIndices getModelInstances( int modelIndex ) const {
		InstanceIndices indices;

		for( int instanceIndex = 0 ; instanceIndex < scene->objects.size() ; ++instanceIndex ) {
			if( scene->objects[ instanceIndex ].modelId == modelIndex ) {
				indices.push_back( instanceIndex );
			}
		}
		for( int instanceIndex = 0 ; instanceIndex < instances.size() ; ++instanceIndex ) {
			if( instances[ instanceIndex ].modelId == modelIndex ) {
				indices.push_back( GLsizei( scene->objects.size() + instanceIndex ) );
			}
		}

		return indices;
	}

};
