#include "sgsSceneRenderer.h"
#include "optixRenderer.h"
#include "optixProgramHelpers.h"
#include "boost/timer/timer.hpp"
#include <boost/random.hpp>

template< typename T >
T *getVectorData( std::vector< T > &container ) {
	if( container.empty() ) {
		return nullptr;
	}
	return &container.front();
}

void SGSSceneRenderer::initOptix( OptixRenderer *optixRenderer ) {
	OptixHelpers::Namespace::Modules terrainModule = OptixHelpers::Namespace::makeModules( "cuda_compile_ptx_generated_terrainMesh.cu.ptx" );
	OptixHelpers::Namespace::Modules objectModule = OptixHelpers::Namespace::makeModules( "cuda_compile_ptx_generated_objectMesh.cu.ptx" );

	// setup the textures
	{
		optix.objectTextureSampler = optixRenderer->context->createTextureSamplerFromGLImage( mergedTexture.handle, RT_TARGET_GL_TEXTURE_2D );
		optix.objectTextureSampler->setWrapMode( 0, RT_WRAP_REPEAT );
		optix.objectTextureSampler->setWrapMode( 1, RT_WRAP_REPEAT );
		// we can still use tex2d here and linear interpolation with array indexing [9/19/2012 kirschan2]
		optix.objectTextureSampler->setIndexingMode( RT_TEXTURE_INDEX_ARRAY_INDEX );
		optix.objectTextureSampler->setReadMode( RT_TEXTURE_READ_NORMALIZED_FLOAT );
		optix.objectTextureSampler->setMaxAnisotropy( 1.0f );
		optix.objectTextureSampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );
	}
	{
		optix.terrainTextureSampler = optixRenderer->context->createTextureSamplerFromGLImage( bakedTerrainTexture.handle, RT_TARGET_GL_TEXTURE_2D );
		optix.terrainTextureSampler->setWrapMode( 0, RT_WRAP_REPEAT );
		optix.terrainTextureSampler->setWrapMode( 1, RT_WRAP_REPEAT );
		optix.terrainTextureSampler->setIndexingMode( RT_TEXTURE_INDEX_NORMALIZED_COORDINATES );
		optix.terrainTextureSampler->setReadMode( RT_TEXTURE_READ_NORMALIZED_FLOAT );
		optix.terrainTextureSampler->setMaxAnisotropy( 1.0f );
		optix.terrainTextureSampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );
	}

	// setup the materials
	{
		optix.objectMaterial = optixRenderer->context->createMaterial ();
		OptixHelpers::Namespace::setMaterialPrograms( optix.objectMaterial, objectModule, "" );
		optix.objectMaterial->validate ();

		optix.objectMaterial[ "objectTexture" ]->setTextureSampler( optix.objectTextureSampler );

		optix.textureInfos = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, mergedTextureInfos.size() );
		optix.textureInfos->setElementSize( sizeof MergedTextureInfo );
		::memcpy( optix.textureInfos->map(), getVectorData( mergedTextureInfos ), sizeof( MergedTextureInfo ) * mergedTextureInfos.size() );
		optix.textureInfos->unmap();
		optix.textureInfos->validate();

		optix.objectMaterial[ "textureInfos" ]->set( optix.textureInfos );
	}
	{
		optix.terrainMaterial = optixRenderer->context->createMaterial ();
		OptixHelpers::Namespace::setMaterialPrograms( optix.terrainMaterial, terrainModule, "" );
		optix.terrainMaterial->validate ();

		optix.terrainMaterial[ "terrainTexture" ]->setTextureSampler( optix.terrainTextureSampler );
	}

	// setup the static objects buffers
#if 0
	{
		int primitiveCount = scene->numSceneIndices / 3;

		optix.staticObjects.indexBuffer = optixRenderer->context->createBufferFromGLBO( RT_BUFFER_INPUT, staticObjectsMesh.indexBuffer.handle );
		optix.staticObjects.indexBuffer->setFormat( RT_FORMAT_UNSIGNED_INT3 );
		optix.staticObjects.indexBuffer->setSize( primitiveCount );
		optix.staticObjects.indexBuffer->validate();

		optix.staticObjects.vertexBuffer = optixRenderer->context->createBufferFromGLBO( RT_BUFFER_INPUT, staticObjectsMesh.vertexBuffer.handle );
		optix.staticObjects.vertexBuffer->setSize( scene->numSceneVertices );
		optix.staticObjects.vertexBuffer->setFormat( RT_FORMAT_USER );
		optix.staticObjects.vertexBuffer->setElementSize( sizeof SGSScene::Vertex );
		optix.staticObjects.vertexBuffer->validate();

		optix.staticObjects.geometry = optixRenderer->context->createGeometry ();
		OptixHelpers::Namespace::setGeometryPrograms( optix.staticObjects.geometry, objectModule, "" );
		optix.staticObjects.geometry->setPrimitiveCount ( primitiveCount );
		optix.staticObjects.geometry->validate ();

		optix.staticObjects.geometryInstance = optixRenderer->context->createGeometryInstance (optix.staticObjects.geometry, &optix.objectMaterial, &optix.objectMaterial + 1);
		optix.staticObjects.geometryInstance->validate ();

		optix.staticObjects.geometryInstance[ "vertexBuffer" ]->setBuffer( optix.staticObjects.vertexBuffer );
		optix.staticObjects.geometryInstance[ "indexBuffer" ]->setBuffer( optix.staticObjects.indexBuffer );

		// set materialInfos and materialIndices
		optix.staticObjects.materialInfos = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, scene->subObjects.size() );
		optix.staticObjects.materialInfos->setElementSize( sizeof OptixProgramInterface::MaterialInfo );

		optix.staticObjects.materialIndices = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT, primitiveCount );

		OptixProgramInterface::MaterialInfo *materialInfos = (OptixProgramInterface::MaterialInfo *) optix.staticObjects.materialInfos->map();
		int *materialIndices = (int *) optix.staticObjects.materialIndices->map();

		for( int objectIndex = 0 ; objectIndex < scene->numSceneObjects ; ++objectIndex ) {
			const SGSScene::Object &object = scene->objects[objectIndex];

			const int endSubObject = object.startSubObject + object.numSubObjects;
			for( int subObjectIndex = object.startSubObject ; subObjectIndex < endSubObject ; ++subObjectIndex ) {
				const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

				OptixProgramInterface::MaterialInfo &materialInfo = materialInfos[subObjectIndex];
				materialInfo.modelIndex = object.modelId;
				materialInfo.objectIndex = objectIndex;

				materialInfo.textureIndex = subObject.material.textureIndex[0];
				materialInfo.alphaType = (OptixProgramInterface::MaterialInfo::AlphaType) subObject.material.alphaType;
				materialInfo.alpha = subObject.material.alpha / 255.0f;
				materialInfo.diffuse.x = subObject.material.diffuse.r / 255.0f;
				materialInfo.diffuse.y = subObject.material.diffuse.g / 255.0f;
				materialInfo.diffuse.z = subObject.material.diffuse.b / 255.0f;

				const int startPrimitive = subObject.startIndex / 3;
				const int endPrimitive = startPrimitive + subObject.numIndices / 3;
				std::fill( materialIndices + startPrimitive, materialIndices + endPrimitive, subObjectIndex );
			}
		}
		optix.staticObjects.materialIndices->unmap();
		optix.staticObjects.materialIndices->validate();

		optix.staticObjects.materialInfos->unmap();
		optix.staticObjects.materialInfos->validate();

		optix.staticObjects.geometryInstance[ "materialIndices" ]->set( optix.staticObjects.materialIndices );
		optix.staticObjects.geometryInstance[ "materialInfos" ]->set( optix.staticObjects.materialInfos );
	}
#endif
	initObjectMeshData( optixRenderer, objectModule, optix.staticObjects );
	refillOptixBuffer( 0, scene->objects.size(), optix.staticObjects );

	// prepare the dynamic buffers
	initObjectMeshData( optixRenderer, objectModule, optix.dynamicObjects );

	// set up the terrain buffers
	bool hasTerrain = !scene->terrain.indices.empty();
	if( hasTerrain ){
		int primitiveCount = scene->terrain.indices.size() / 3;
		optix.terrain.indexBuffer = optixRenderer->context->createBufferFromGLBO( RT_BUFFER_INPUT, terrainMesh.indexBuffer.handle );
		optix.terrain.indexBuffer->setFormat( RT_FORMAT_UNSIGNED_INT3 );
		optix.terrain.indexBuffer->setSize( primitiveCount );
		optix.terrain.indexBuffer->validate();

		optix.terrain.vertexBuffer = optixRenderer->context->createBufferFromGLBO( RT_BUFFER_INPUT, terrainMesh.vertexBuffer.handle );
		optix.terrain.vertexBuffer->setSize( scene->terrain.vertices.size() );
		optix.terrain.vertexBuffer->setFormat( RT_FORMAT_USER );
		optix.terrain.vertexBuffer->setElementSize( sizeof SGSScene::Terrain::Vertex );
		optix.terrain.vertexBuffer->validate();

		optix.terrain.geometry = optixRenderer->context->createGeometry ();
		optix.terrain.geometry ["vertexBuffer"]->setBuffer (optix.terrain.vertexBuffer);
		optix.terrain.geometry ["indexBuffer"]->setBuffer (optix.terrain.indexBuffer);
		OptixHelpers::Namespace::setGeometryPrograms( optix.terrain.geometry, terrainModule, "" );

		optix.terrain.geometry->setPrimitiveCount ( primitiveCount );
		optix.terrain.geometry->validate ();

		optix.terrain.geometryInstance = optixRenderer->context->createGeometryInstance (optix.terrain.geometry, &optix.terrainMaterial, &optix.terrainMaterial + 1);
		optix.terrain.geometryInstance->validate ();
	}

	optix.staticAcceleration = optixRenderer->context->createAcceleration ("Sbvh", "Bvh");
	optix.dynamicAcceleration = optixRenderer->context->createAcceleration ("Lbvh", "Bvh");

	BOOST_STATIC_ASSERT( sizeof( SGSScene::Vertex ) == sizeof( SGSScene::Terrain::Vertex) );
	/*optix.staticAcceleration->setProperty( "vertex_buffer_name", "vertexBuffer" );
	optix.staticAcceleration->setProperty( "vertex_buffer_stride", boost::lexical_cast<std::string>( sizeof( SGSScene::Vertex ) ) );
	optix.staticAcceleration->setProperty( "index_buffer_name", "indexBuffer" );
	*/
	optix.staticAcceleration->validate ();
	optix.dynamicAcceleration->validate ();

	optix.staticScene = optixRenderer->context->createGeometryGroup();
	optix.staticScene->setAcceleration (optix.staticAcceleration);
	
	{
		int numChildren = 1;
		if( hasTerrain ) {
			numChildren++;
		}

		optix.staticScene->setChildCount( numChildren );
		optix.staticScene->setChild ( --numChildren, optix.staticObjects.geometryInstance );
		if( hasTerrain ) {
			optix.staticScene->setChild ( numChildren, optix.terrain.geometryInstance );
		}
		optix.staticScene->validate ();

		optixRenderer->addSceneChild( optix.staticScene );
	}
	

	optix.dynamicScene = optixRenderer->context->createGeometryGroup();
	optix.dynamicScene->setAcceleration( optix.dynamicAcceleration );
	optix.dynamicScene->setChildCount( 0 );
	optix.dynamicScene->validate();

	optixRenderer->addSceneChild( optix.dynamicScene );
}

static const char *optixCacheFilename = "scene.optixCache";

bool SGSSceneRenderer::loadOptixCache() {
	Optix::Cache cache;

	{
		boost::timer::auto_cpu_timer timer( "initOptix; load cache: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		Serializer::BinaryReader reader( optixCacheFilename, Optix::Cache::VERSION );
		if( reader.valid() ) {
			reader.get( cache );
		}

		if( cache.magicStamp != getSceneHash() ) {
			cache = Optix::Cache();
//			cache.magicStamp = scene->numSceneVertices;
		}
	}

	if( !cache.staticSceneAccelerationCache.empty() ) {
		// load from the cache
		optix.staticAcceleration->setData( &cache.staticSceneAccelerationCache.front(), cache.staticSceneAccelerationCache.size() );
		return true;
	} else {
		return false;
	}
}

void SGSSceneRenderer::writeOptixCache() {
	Optix::Cache cache;
	cache.magicStamp = getSceneHash();

	int size;
	size = optix.staticAcceleration->getDataSize();
	cache.staticSceneAccelerationCache.resize( size );
	optix.staticAcceleration->getData( &cache.staticSceneAccelerationCache.front() );

	boost::timer::auto_cpu_timer timer( "initOptix; write cache: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

	Serializer::BinaryWriter writer( optixCacheFilename, Optix::Cache::VERSION );
	writer.put( cache );
}

void SGSSceneRenderer::refillDynamicOptixBuffers() {
	refillOptixBuffer( scene->objects.size(), scene->objects.size() + instances.size(), optix.dynamicObjects );

	// register the dynamic scene
	if( !instances.empty() ) {
		optix.dynamicScene->setChildCount( 1 );
		optix.dynamicScene->setChild( 0, optix.dynamicObjects.geometryInstance );
	}
	else {
		optix.dynamicScene->setChildCount( 0 );
	}

	// the acceleration structure is invalid now
	optix.dynamicAcceleration->markDirty();

	optix.dynamicBufferDirty = false;
}

void OptixRenderer::init( const std::shared_ptr< SGSSceneRenderer > &sgsSceneRenderer ) {
	this->sgsSceneRenderer = sgsSceneRenderer;

	context = optix::Context::create();

	// initialize the context
	context->setRayTypeCount( OptixProgramInterface::RT_COUNT );
	context->setEntryPointCount( OptixProgramInterface::EP_COUNT );
	context->setCPUNumThreads( 12 );
	context->setStackSize(8000);
	context->setPrintBufferSize(65536);
	context->setPrintEnabled(true);

	// create the rt programs
	static const char *raytracerPtxFilename = "cuda_compile_ptx_generated_raytracer.cu.ptx";
	static const char *probeSamplerPtxFilename = "cuda_compile_ptx_generated_probeTracer.cu.ptx";
	static const char *pinholeSelectionPtxFilename = "cuda_compile_ptx_generated_pinholeSelection.cu.ptx";

	OptixHelpers::Namespace::Modules modules = OptixHelpers::Namespace::makeModules( raytracerPtxFilename, probeSamplerPtxFilename, pinholeSelectionPtxFilename );

	OptixHelpers::Namespace::setRayGenerationPrograms( context, modules );
	OptixHelpers::Namespace::setMissPrograms( context, modules );
	OptixHelpers::Namespace::setExceptionPrograms( context, modules );

	// create the acceleration structure
	acceleration = context->createAcceleration ("NoAccel", "NoAccel");

	// create the scene node
	scene = context->createGroup ();
	scene->setAcceleration (acceleration);

	context["rootObject"]->set(scene);

	// create and set the output buffer
	outputBuffer = context->createBuffer( RT_BUFFER_OUTPUT,  RT_FORMAT_UNSIGNED_BYTE4, width = 640, height = 480 );
	context["outputBuffer"]->set(outputBuffer);

	// create and set the hemisphere sample buffer
	hemisphereSamples = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, numHemisphereSamples );
	createHemisphereSamples( (optix::float3*) hemisphereSamples->map() );
	hemisphereSamples->unmap();
	hemisphereSamples->validate();

	context[ "hemisphereSamples" ]->set( hemisphereSamples );

	// create and set the probe buffers
	probes = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, maxNumProbes );
	probes->setElementSize( sizeof OptixProgramInterface::Probe );
	probes->validate();

	context[ "probes" ]->set( probes );

	probeContexts = context->createBuffer( RT_BUFFER_OUTPUT, RT_FORMAT_USER, maxNumProbes );
	probeContexts->setElementSize( sizeof OptixProgramInterface::ProbeContext );
	probeContexts->validate();

	context[ "probeContexts" ]->set( probeContexts );

	// init and set the selection buffers
	selectionRays = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, maxNumSelectionRays );
	selectionRays->validate();

	context[ "selectionRays" ]->set( selectionRays );

	selectionResults = context->createBuffer( RT_BUFFER_OUTPUT, RT_FORMAT_USER, maxNumSelectionRays );
	selectionResults->setElementSize( sizeof OptixProgramInterface::SelectionResult );
	selectionResults->validate();

	context[ "selectionResults" ]->set( selectionResults );

	// init the output texture
	SimpleGL::setLinearMinMag( debugTexture );

	// validate all objects
	scene->validate ();
	acceleration->validate();
	context->validate();

	// init our main object source
	sgsSceneRenderer->initOptix( this );

	bool cacheLoaded = sgsSceneRenderer->loadOptixCache();

	compileContext();

	if( !cacheLoaded ) {
		sgsSceneRenderer->writeOptixCache();
	}
}

void OptixRenderer::setRenderContext( const RenderContext &renderContext ) {
	context[ "disabledModelIndex" ]->setInt( renderContext.disabledModelIndex );
	context[ "disabledObjectIndex" ]->setInt( renderContext.disabledInstanceIndex );
}

void OptixRenderer::setPinholeCameraViewerContext( const ViewerContext &viewerContext ) {
	context[ "eyePosition" ]->set3fv( viewerContext.worldViewerPosition.data() );

	// this works with all usual projection matrices (where x and y don't have any effect on z and w in clip space)
	// determine u, v, and w by unprojecting (x,y,-1,1) from clip space to world space
	Eigen::Matrix4f inverseProjectionView = viewerContext.projectionView.inverse();
	// this is the w coordinate of the unprojected coordinates
	const float unprojectedW = inverseProjectionView(3,3) - inverseProjectionView(3,2);

	// divide the homogeneous affine matrix by the projected w
	// see R1 page for deduction
	Eigen::Matrix< float, 3, 4> inverseProjectionView34 = viewerContext.projectionView.inverse().topLeftCorner<3,4>() / unprojectedW;
	const Eigen::Vector3f u = inverseProjectionView34.col(0);
	const Eigen::Vector3f v = inverseProjectionView34.col(1);
	const Eigen::Vector3f w = inverseProjectionView34.col(3) - inverseProjectionView34.col(2) - viewerContext.worldViewerPosition;

	context[ "U" ]->set3fv( u.data() );
	context[ "V" ]->set3fv( v.data() );
	context[ "W" ]->set3fv( w.data() );
}

void OptixRenderer::renderPinholeCamera( const ViewerContext &viewerContext, const RenderContext &renderContext ) {
	prepareLaunch();

	setRenderContext( renderContext );
	setPinholeCameraViewerContext( viewerContext );

	context->launch( OptixProgramInterface::renderPinholeCameraView, width, height );

	void *data = outputBuffer->map();
	debugTexture.load( 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	//SOIL_save_image( "frame.bmp", SOIL_SAVE_TYPE_BMP, 640, 480, 4, (unsigned char*) data );
	outputBuffer->unmap();
}

void OptixRenderer::selectFromPinholeCamera( const std::vector< optix::float2 > &selectionRays, std::vector< OptixProgramInterface::SelectionResult > &selectionResults, const ViewerContext &viewerContext, const RenderContext &renderContext ) {
	prepareLaunch();

	// check bounds
	if( selectionRays.size() > maxNumSelectionRays ) {
		throw std::invalid_argument( "too many selection rays!" );
	}

	setRenderContext( renderContext );
	setPinholeCameraViewerContext( viewerContext );

	OptixHelpers::Buffer::copyToDevice( this->selectionRays, selectionRays );

	context->launch( OptixProgramInterface::selectFromPinholeCamera, selectionRays.size() );

	selectionResults.resize( selectionRays.size() );
	OptixHelpers::Buffer::copyToHost( this->selectionResults, selectionResults.front(), selectionResults.size() );
}

void OptixRenderer::sampleProbes( const std::vector< Probe > &probes, std::vector< ProbeContext > &probeContexts, const RenderContext &renderContext, float maxDistance, int sampleOffset ) {
	prepareLaunch();

	// check bounds
	if( probes.size() > maxNumProbes ) {
		throw std::invalid_argument( "too many probes!" );
	}
	if( probes.size() == 0 ) {
		throw std::invalid_argument( "no probes!" );
	}

	setRenderContext( renderContext );

	context[ "maxDistance" ]->setFloat( maxDistance );
	context[ "sampleOffset" ]->setUint( sampleOffset );

	OptixHelpers::Buffer::copyToDevice( this->probes, probes );

	context->launch( OptixProgramInterface::sampleProbes, probes.size() );

	probeContexts.resize( probes.size() );
	OptixHelpers::Buffer::copyToHost( this->probeContexts, probeContexts.front(), probeContexts.size() );
}

void OptixRenderer::compileContext()  {
	auto r = rtContextCompile (context->get());
	const char* e;
	rtContextGetErrorString (context->get(), r, &e);
	std::cout << e;

	{
		boost::timer::auto_cpu_timer timer( "compileContext; test launch all entry points: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		// build the static scene acceleration tree
		context->launch( OptixProgramInterface::renderPinholeCameraView, 0, 0 );
		context->launch( OptixProgramInterface::sampleProbes, 0 );
		context->launch( OptixProgramInterface::selectFromPinholeCamera, 0 );
	}
}

void OptixRenderer::createHemisphereSamples( optix::float3 *hemisphereSamples ) {
	// produces randomness out of thin air
	boost::random::mt19937 rng;
	// see pseudo-random number generators
	boost::random::uniform_01<> distribution;


	// info about how cosine_sample_hemisphere's parameters work
	// we sample a disk and project it up onto the hemisphere
	//
	// u1 is the radius and u2 the angle

	// we have 8 sample directions in every unit circle slice
	// => fov: 45° => half is 22.5
	// // sin(22.5°) = 0.38268343236
	for( int i = 0 ; i < numHemisphereSamples ; ++i ) {
		const float u1 = distribution(rng) * 0.38268343236;
		const float u2 = distribution(rng);
		optix::cosine_sample_hemisphere( u1, u2, hemisphereSamples[i] );
	}
}

void SGSSceneRenderer::initObjectMeshData( OptixRenderer *optixRenderer, const OptixHelpers::Namespace::Modules &modules, Optix::ObjectMeshData &meshData ) {
	meshData.indexBuffer = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_INT3, 1 );
	meshData.indexBuffer->validate();

	meshData.vertexBuffer = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, 1 );
	meshData.vertexBuffer->setElementSize( sizeof SGSScene::Vertex );
	meshData.vertexBuffer->validate();

	meshData.geometry = optixRenderer->context->createGeometry ();
	OptixHelpers::Namespace::setGeometryPrograms( meshData.geometry, modules, "" );
	meshData.geometry->setPrimitiveCount ( 0 );
	meshData.geometry->validate ();

	meshData.geometryInstance = optixRenderer->context->createGeometryInstance (meshData.geometry, &optix.objectMaterial, &optix.objectMaterial + 1);
	meshData.geometryInstance->validate ();

	meshData.geometryInstance[ "vertexBuffer" ]->setBuffer( meshData.vertexBuffer );
	meshData.geometryInstance[ "indexBuffer" ]->setBuffer( meshData.indexBuffer );

	// set materialInfos and materialIndices
	meshData.materialInfos = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, 0 );
	meshData.materialInfos->setElementSize( sizeof OptixProgramInterface::MaterialInfo );

	meshData.materialIndices = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT, 0 );

	meshData.materialIndices->validate();
	meshData.materialInfos->validate();

	meshData.geometryInstance[ "materialIndices" ]->set( meshData.materialIndices );
	meshData.geometryInstance[ "materialInfos" ]->set( meshData.materialInfos );
}

void SGSSceneRenderer::refillOptixBuffer( const int beginInstanceIndex, const int endInstanceIndex, Optix::ObjectMeshData &meshData ) {
	int numVertices = 0;
	int numIndices = 0;
	int numSubObjects = 0;

	// set numVertices, numIndices and numSubObjects
	for( int instanceIndex = beginInstanceIndex ; instanceIndex < endInstanceIndex ; instanceIndex++ ) {
		const SGSScene::Model &model = scene->models[ getModelIndex( instanceIndex ) ];

		numSubObjects += model.numSubObjects;
		const int endSubObject = model.startSubObject + model.numSubObjects;
		for( int subObjectIndex = model.startSubObject ; subObjectIndex < endSubObject ; subObjectIndex++ ) {
			const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];
			numVertices += subObject.numVertices;
			numIndices += subObject.numIndices;
		}
	}

	// resize the buffers
	meshData.indexBuffer->setSize( numIndices );
	meshData.indexBuffer->validate();

	meshData.vertexBuffer->setSize( numVertices );
	meshData.vertexBuffer->validate();

	const int numPrimitives = numIndices / 3;
	meshData.geometry->setPrimitiveCount ( numPrimitives );

	meshData.materialInfos->setSize( numSubObjects );
	meshData.materialIndices->setSize( numPrimitives );

	auto materialInfos = (OptixProgramInterface::MaterialInfo *) meshData.materialInfos->map();
	auto materialIndices = (int *) meshData.materialIndices->map();
	auto vertices = (SGSScene::Vertex *) meshData.vertexBuffer->map();
	auto indices = (unsigned int *) meshData.indexBuffer->map();

	int vertexBufferIndex = 0;
	int indexBufferIndex = 0;
	int globalSubObjectIndex = 0;

	// compile all instances into the dynamic buffers
	for( int instanceIndex = beginInstanceIndex ; instanceIndex < endInstanceIndex ; instanceIndex++ ) {
		const int modelIndex = getModelIndex( instanceIndex );
		const SGSScene::Model &model = getModel( modelIndex );

		const auto instanceTransformation = getInstanceTransformation( instanceIndex );
		const Eigen::Matrix3f normalTransformation = instanceTransformation.linear().inverse().transpose();

		const int endSubObject = model.startSubObject + model.numSubObjects;
		for( int subObjectIndex = model.startSubObject ; subObjectIndex < endSubObject ; ++subObjectIndex ) {
			const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

			// set the material info
			OptixProgramInterface::MaterialInfo &materialInfo = materialInfos[ globalSubObjectIndex ];
			materialInfo.modelIndex = modelIndex;
			//  rename this to instanceIndex [10/18/2012 kirschan2]
			materialInfo.objectIndex = instanceIndex;

			materialInfo.textureIndex = subObject.material.textureIndex[0];
			materialInfo.alphaType = (OptixProgramInterface::MaterialInfo::AlphaType) subObject.material.alphaType;
			materialInfo.alpha = subObject.material.alpha / 255.0f;

			materialInfo.diffuse.x = subObject.material.diffuse.r / 255.0f;
			materialInfo.diffuse.y = subObject.material.diffuse.g / 255.0f;
			materialInfo.diffuse.z = subObject.material.diffuse.b / 255.0f;

			// set the material indices
			const int startPrimitive = indexBufferIndex / 3;
			const int endPrimitive = startPrimitive + subObject.numIndices / 3;
			std::fill( materialIndices + startPrimitive, materialIndices + endPrimitive, globalSubObjectIndex );

			int startVertex = vertexBufferIndex;

			// copy vertices
			for( int vertexIndex = subObject.startVertex ; vertexIndex < subObject.startVertex + subObject.numVertices ; ++vertexIndex ) {
				auto &modelVertex = scene->vertices[ vertexIndex ];
				auto &instanceVertex = vertices[ vertexBufferIndex++ ];

				instanceVertex = modelVertex;
				Eigen::Vector3f::Map( instanceVertex.position ) = instanceTransformation * Eigen::Vector3f::Map( instanceVertex.position );
				Eigen::Vector3f::Map( instanceVertex.normal ) = normalTransformation * Eigen::Vector3f::Map( instanceVertex.normal );
			}

			// copy indices
			int indexShift = startVertex - subObject.startVertex;
			for( int indexIndex = subObject.startIndex ; indexIndex < subObject.startIndex + subObject.numIndices ; ++indexIndex ) {
				indices[ indexBufferIndex++ ] = scene->indices[ indexIndex ] + indexShift;
			}

			++globalSubObjectIndex;
		}
	}
	meshData.materialIndices->unmap();
	meshData.materialInfos->unmap();
	meshData.vertexBuffer->unmap();
	meshData.indexBuffer->unmap();
}

void OptixRenderer::prepareLaunch() {
	if( sgsSceneRenderer->optix.dynamicBufferDirty ) {
		sgsSceneRenderer->refillDynamicOptixBuffers();
		acceleration->markDirty();
	}
}