#pragma once

#include <gl/glew.h>
#include <SOIL.h>

#include <optix_world.h>

#include "glObjectWrappers.h"
#include <debugRender.h>
#include "glslPipeline.h"

#include "sgsScene.h"

#include "optixProgramInterface.h"
SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::MergedTextureInfo );

#include <Eigen/Eigen>

#include "eigenProjectionMatrices.h"

#include <boost/random.hpp>
#include "boost/range/algorithm/copy.hpp"

#include <contextHelper.h>
#include "boost/format.hpp"
#include <boost/algorithm/string/join.hpp>
#include <vector>

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

//////////////////////////////////////////////////////////////////////////
struct SGSSceneRenderer;

struct RenderContext {
	int disabledInstanceIndex;
	int disabledObjectIndex;

	void setDefault() {
		disabledInstanceIndex = -1;
		disabledObjectIndex = -1;
	}
};

struct ViewerContext {
	Eigen::Matrix4f projectionView;
	Eigen::Vector3f worldViewerPosition;
};

namespace OptixHelpers {
	namespace Buffer {
		// TODO: use arrayref
		// v is presized
		template< typename T >
		void copyToHost( optix::Buffer &buffer, T &first, int count ) {
			const T *start = (const T *) buffer->map();
			std::copy( start, start + count, &first );
			buffer->unmap();
		}

		// TODO: use arrayref
		template< typename T >
		void copyToDevice( optix::Buffer &buffer, const std::vector< T > &v ) {
			boost::range::copy( v, (T *) buffer->map() );
			buffer->unmap();
		}
	}

	namespace Namespace {
		typedef std::vector< std::string > Modules;

		inline Modules makeModules( const char *filename ) {
			Modules modules;
			modules.push_back( filename );
			return modules;
		}

		inline Modules makeModules( const char *filenameA, const char *filenameB ) {
			Modules modules;
			modules.push_back( filenameA );
			modules.push_back( filenameB );
			return modules;
		}

		inline Modules makeModules( const char *filenameA, const char *filenameB, const char *filenameC ) {
			Modules modules;
			modules.push_back( filenameA );
			modules.push_back( filenameB );
			modules.push_back( filenameC );
			return modules;
		}

		inline std::string getRayProgramName( unsigned rayType, const char *name ) {
			return ::boost::str( ::boost::format( "%s_%s" ) % OptixProgramInterface::rayTypeNamespaces[ rayType ] % name );
		}

		// supports namespaces
		inline std::string getMaterialProgramName( const std::string &materialName, unsigned rayType, const char *name ) {
			if( !materialName.empty() ) {
				return ::boost::str( ::boost::format( "%s::%s_%s" ) % materialName % OptixProgramInterface::rayTypeNamespaces[ rayType ] % name );
			}
			else {
				return ::boost::str( ::boost::format( "%s_%s" ) % OptixProgramInterface::rayTypeNamespaces[ rayType ] % name );
			}
		}

		// supports optional namespaces
		inline std::string getGeometryProgramName( const std::string &geometryName, const char *name ) {
			if( !geometryName.empty() ) {
				return ::boost::str( ::boost::format( "%s::%s" ) % geometryName % name );
			}
			else {
				return ::boost::str( ::boost::format( "%s" ) % name );
			}
		}

		inline std::string getEntryPointProgramName( unsigned entryPoint, const char *name ) {
			return ::boost::str( ::boost::format( "%s_%s" ) % OptixProgramInterface::entryPointNamespaces[ entryPoint ] % name );
		}

		inline bool getProgram( optix::Context &context, optix::Program &program, const Modules &modules, const std::string fullName ) {
			for( int moduleIndex = 0 ; moduleIndex < modules.size() ; moduleIndex++ ) {
				RTprogram innerProgram;
				
				RTresult result = rtProgramCreateFromPTXFile( context->get(), modules[ moduleIndex ].c_str(), fullName.c_str(), &innerProgram );
				
				// function not found?
				if( result == RT_ERROR_INVALID_SOURCE ) {
					continue;
				}
				else if( result == RT_SUCCESS ) {
					std::cout << boost::format( "%s: found '%s'\n" ) % modules[ moduleIndex ] % fullName;
					program = optix::Program::take( innerProgram );
					return true;
				}
				else {
					context->checkError( result );
				}
			}
			
			std::cout << boost::format( "'%s' not found in %s\n" ) % fullName % boost::algorithm::join( modules, ", " );
			return false;
		}

#define create_setMaterialProgram( name, ptxName ) \
		inline void set ##name ##Programs( optix::Material &material, const Modules &modules, const std::string &materialName ) { \
			for( unsigned int rayType = 0 ; rayType < OptixProgramInterface::RT_COUNT ; ++rayType ) { \
				optix::Program program; \
				if( getProgram( material->getContext(), program, modules, getMaterialProgramName( materialName, rayType, #ptxName ) ) ) { \
					material->set ##name ##Program( (unsigned) rayType, program ); \
				} \
			} \
		}

		create_setMaterialProgram( AnyHit, anyHit )
		create_setMaterialProgram( ClosestHit, closestHit )
#undef create_setMaterialProgram

		inline void setMaterialPrograms( optix::Material &material, const Modules &modules, const std::string &materialName ) {
			setAnyHitPrograms( material, modules, materialName );
			setClosestHitPrograms( material, modules, materialName );
		}

		inline void setMissPrograms( optix::Context &context, const Modules &modules ) {
			for( unsigned int rayType = 0 ; rayType < OptixProgramInterface::RT_COUNT ; ++rayType ) {
				optix::Program program;
				if( getProgram( context, program, modules, getRayProgramName( rayType, "miss" ) ) ) {
					context->setMissProgram( (unsigned) rayType, program );
				}
			}
		}

		inline void setIntersectionProgram( optix::Geometry &geometry, const Modules &modules, const std::string &geometryName ) {
			optix::Program program;
			if( getProgram( geometry->getContext(), program, modules, getGeometryProgramName( geometryName, "intersect" ) ) ) {
				geometry->setIntersectionProgram( program );
			}
		}

		inline void setBoundingBoxProgram( optix::Geometry &geometry, const Modules &modules, const std::string &geometryName ) {
			optix::Program program;
			if( getProgram( geometry->getContext(), program, modules, getGeometryProgramName( geometryName, "calculateBoundingBox" ) ) ) {
				geometry->setBoundingBoxProgram( program );
			}
		}

		inline void setGeometryPrograms( optix::Geometry &geometry, const Modules &modules, const std::string &geometryName ) {
			setIntersectionProgram( geometry, modules, geometryName );
			setBoundingBoxProgram( geometry, modules, geometryName );
		}

		inline void setRayGenerationPrograms( optix::Context &context, const Modules &modules ) {
			for( unsigned int entryPoint = 0 ; entryPoint < OptixProgramInterface::EP_COUNT ; ++entryPoint ) {
				optix::Program program;
				if( getProgram( context, program, modules, OptixProgramInterface::entryPointNamespaces[ entryPoint ] ) ) {
					context->setRayGenerationProgram( entryPoint, program );
				}
			}
		}

		inline void setExceptionPrograms( optix::Context &context, const Modules &modules ) {
			for( unsigned int entryPoint = 0 ; entryPoint < OptixProgramInterface::EP_COUNT ; ++entryPoint ) {
				optix::Program program;
				if( getProgram( context, program, modules, getEntryPointProgramName( entryPoint, "exception" ) ) ) {
					context->setExceptionProgram( entryPoint, program );
				}
			}
		}
		
		inline void setEntryPointPrograms( optix::Context &context, const Modules &modules ) {
			setRayGenerationPrograms( context, modules );
			setExceptionPrograms( context, modules );
		}
	}
}

// TODO: -> own file [9/20/2012 kirschan2]
struct OptixRenderer {
	typedef OptixProgramInterface::Probe Probe;
	typedef OptixProgramInterface::ProbeContext ProbeContext;

	typedef std::vector< optix::float2 > SelectionRays;

	typedef OptixProgramInterface::SelectionResult SelectionResult;
	typedef std::vector<SelectionResult > SelectionResults;

	static const int numHemisphereSamples = 39989;
	static const int maxNumProbes = 8192;
	static const int maxNumSelectionRays = 32;

	optix::Context context;
	optix::Group scene;
	optix::Acceleration acceleration;

	optix::Buffer outputBuffer;
	int width, height;
	ScopedTexture2D debugTexture;

	optix::Buffer probes, probeContexts, hemisphereSamples;

	optix::Buffer selectionRays, selectionResults;

	std::shared_ptr< SGSSceneRenderer > sgsSceneRenderer;

	void init( const std::shared_ptr< SGSSceneRenderer > &sgsSceneRenderer );
	
	void createHemisphereSamples( optix::float3 *hemisphereSamples ) {
		// produces randomness out of thin air
		boost::random::mt19937 rng;
		// see pseudo-random number generators
		boost::random::uniform_01<> distribution;

		for( int i = 0 ; i < numHemisphereSamples ; ++i ) {
			const float u1 = distribution(rng) * 0.25f;
			const float u2 = distribution(rng);
			optix::cosine_sample_hemisphere( u1, u2, hemisphereSamples[i] );
		}
	}

	void setRenderContext( const RenderContext &renderContext );
	void setPinholeCameraViewerContext( const ViewerContext &viewerContext );

	void renderPinholeCamera( const ViewerContext &viewerContext, const RenderContext &renderContext );
	void selectFromPinholeCamera( const SelectionRays &selectionRays, SelectionResults &selectionResults, const ViewerContext &viewerContext, const RenderContext &renderContext );
	void sampleProbes( const std::vector< Probe > &probes, std::vector< ProbeContext > &probeContexts, const RenderContext &renderContext );

	void addSceneChild( const optix::GeometryGroup &child ) {
		int index = scene->getChildCount();
		scene->setChildCount( index + 1 );
		scene->setChild( index, child );
	}
};

struct SGSSceneRenderer {
	struct Optix {
		optix::GeometryGroup staticScene;
		optix::Acceleration staticAcceleration;
				
		struct MeshData {
			optix::Buffer indexBuffer, vertexBuffer;
			optix::Geometry geometry;
			optix::GeometryInstance geometryInstance;

			optix::Program intersect, boundingBox;
			optix::Material material;
		};
		MeshData terrain, objects;

		optix::Buffer textureInfos, materialInfos, materialIndices;
		optix::TextureSampler terrainTextureSampler;
		optix::TextureSampler objectTextureSampler;

		struct Cache {
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
	Program terrainProgram, objectProgram, shadowMapProgram;

	struct Debug {
		bool showBoundingSpheres, showTerrainBoundingSpheres;
		DebugRender::CombinedCalls boundingSpheres, terrainBoundingSpheres;

		bool updateRenderLists;

		Debug() : showBoundingSpheres( false ), showTerrainBoundingSpheres( false ), updateRenderLists( true ) {}
	} debug;

	std::vector<int> solidLists;
	std::vector<int> alphaLists;
	std::vector<int> terrainLists;

	GL::ScopedBuffer objectVertices, objectIndices, terrainVertices, terrainIndices;
	GL::ScopedVertexArrayObject objectVAO, terrainVAO;
	// one display list per sub object
	GL::ScopedDisplayLists materialDisplayLists;

	struct Cache {
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

	void bakeTerrainTexture( int detailFactor, float textureDetailFactor );

	void reloadShaders();

	void processScene( const std::shared_ptr<SGSScene> &scene, const char *cacheFilename );
	void loadBuffers();
	void setVertexArrayObjects();
	void prepareMaterialDisplayLists();

	// prerender everything into display lists for easy drawing later
	void prerender();

	ScopedTexture2D sunShadowMap;
	Eigen::Matrix4f sunProjectionMatrix;
	int sunShadowMapSize;

	void initShadowMap();
	void initShadowMapProjectionMatrix( const Eigen::AlignedBox3f &boundingBox, const Eigen::Vector3f &direction );
	void renderShadowmap( const RenderContext &renderContext );

	void buildDrawLists( const Eigen::Matrix4f &projectionView, const RenderContext &renderContext );
	void sortAlphaList( const Eigen::Vector3f &worldViewerPosition );

	void render( const Eigen::Matrix4f &projectionView, const Eigen::Vector3f &worldViewerPosition, const RenderContext &renderContext );

	void initOptix( OptixRenderer *optixRenderer );
};