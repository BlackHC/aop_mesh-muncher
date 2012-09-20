#define NOMINMAX
#include <Windows.h>
#undef near
#undef far

#include "sgsSceneRender.h"

void SGSSceneRenderer::initOptix( OptixRenderer *optixRenderer ) {
	const char *terrainMeshPtxFilename = "cuda_compile_ptx_generated_terrainMesh.cu.ptx";
	optix.terrain.intersect = optixRenderer->context->createProgramFromPTXFile (terrainMeshPtxFilename, "intersect");
	optix.terrain.boundingBox = optixRenderer->context->createProgramFromPTXFile (terrainMeshPtxFilename, "calculateBoundingBox");
	optix.terrain.anyHit = optixRenderer->context->createProgramFromPTXFile (terrainMeshPtxFilename, "anyHit");
	optix.terrain.closestHit = optixRenderer->context->createProgramFromPTXFile (terrainMeshPtxFilename, "closestHit");

	const char *objectMeshPtxFilename = "cuda_compile_ptx_generated_objectMesh.cu.ptx";
	optix.objects.intersect = optixRenderer->context->createProgramFromPTXFile (objectMeshPtxFilename, "intersect");
	optix.objects.boundingBox = optixRenderer->context->createProgramFromPTXFile (objectMeshPtxFilename, "calculateBoundingBox");
	optix.objects.anyHit = optixRenderer->context->createProgramFromPTXFile (objectMeshPtxFilename, "anyHit");
	optix.objects.closestHit = optixRenderer->context->createProgramFromPTXFile (objectMeshPtxFilename, "closestHit");

	// setup the objects buffers
	{
		optix.objects.material = optixRenderer->context->createMaterial ();
		optix.objects.material->setAnyHitProgram (1, optix.objects.anyHit);
		optix.objects.material->setClosestHitProgram (0, optix.objects.closestHit);
		optix.objects.material->validate ();

		int primitiveCount = scene->numSceneIndices / 3;

		optix.objects.indexBuffer = optixRenderer->context->createBufferFromGLBO( RT_BUFFER_INPUT, objectIndices.handle );
		optix.objects.indexBuffer->setFormat( RT_FORMAT_UNSIGNED_INT3 );
		optix.objects.indexBuffer->setSize( primitiveCount );
		optix.objects.indexBuffer->validate();

		optix.objects.vertexBuffer = optixRenderer->context->createBufferFromGLBO( RT_BUFFER_INPUT, objectVertices.handle );
		optix.objects.vertexBuffer->setSize( scene->numSceneVertices );
		optix.objects.vertexBuffer->setFormat( RT_FORMAT_USER );
		optix.objects.vertexBuffer->setElementSize( sizeof SGSScene::Vertex );
		optix.objects.vertexBuffer->validate();

		optix.objects.geometry = optixRenderer->context->createGeometry ();
		optix.objects.geometry ["vertexBuffer"]->setBuffer (optix.objects.vertexBuffer);
		optix.objects.geometry ["indexBuffer"]->setBuffer (optix.objects.indexBuffer);
		optix.objects.geometry->setBoundingBoxProgram (optix.objects.boundingBox);
		optix.objects.geometry->setIntersectionProgram (optix.objects.intersect);

		optix.objects.geometry->setPrimitiveCount ( primitiveCount );
		optix.objects.geometry->validate ();

		optix.objects.geometryInstance = optixRenderer->context->createGeometryInstance (optix.objects.geometry, &optix.objects.material, &optix.objects.material + 1);
		optix.objects.geometryInstance->validate ();

		optix.objectTextureSampler = optixRenderer->context->createTextureSamplerFromGLImage( mergedTexture.handle, RT_TARGET_GL_TEXTURE_2D );
		optix.objectTextureSampler->setWrapMode( 0, RT_WRAP_REPEAT );
		optix.objectTextureSampler->setWrapMode( 1, RT_WRAP_REPEAT );
		// we can still use tex2d here and linear interpolation with array indexing [9/19/2012 kirschan2]
		optix.objectTextureSampler->setIndexingMode( RT_TEXTURE_INDEX_ARRAY_INDEX );
		optix.objectTextureSampler->setReadMode( RT_TEXTURE_READ_NORMALIZED_FLOAT );
		optix.objectTextureSampler->setMaxAnisotropy( 1.0f );
		optix.objectTextureSampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

		optix.objects.material[ "objectTexture" ]->setTextureSampler( optix.objectTextureSampler );

		optix.textureInfos = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, mergedTextureInfos.size() );
		optix.textureInfos->setElementSize( sizeof MergedTextureInfo );
		::memcpy( optix.textureInfos->map(), &mergedTextureInfos.front(), sizeof( MergedTextureInfo ) * mergedTextureInfos.size() );
		optix.textureInfos->unmap();
		optix.textureInfos->validate();

		// set materialInfos and materialIndices
		optix.materialInfos = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, scene->subObjects.size() );
		optix.materialInfos->setElementSize( sizeof OptixProgramInterface::MaterialInfo );

		optix.materialIndices = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT, primitiveCount );

		OptixProgramInterface::MaterialInfo *materialInfos = (OptixProgramInterface::MaterialInfo *) optix.materialInfos->map();	
		int *materialIndices = (int *) optix.materialIndices->map();

		for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
			const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

			OptixProgramInterface::MaterialInfo &materialInfo = materialInfos[subObjectIndex];
			materialInfo.textureIndex = subObject.material.textureIndex[0];
			materialInfo.alphaType = (OptixProgramInterface::MaterialInfo::AlphaType) subObject.material.alphaType;
			materialInfo.alpha = subObject.material.alpha / 255.0f;

			const int startPrimitive = subObject.startIndex / 3;
			const int endPrimitive = startPrimitive + subObject.numIndices / 3;
			std::fill( materialIndices + startPrimitive, materialIndices + endPrimitive, subObjectIndex );
		}
		optix.materialIndices->unmap();
		optix.materialIndices->validate();

		optix.materialInfos->unmap();
		optix.materialInfos->validate();

		optixRenderer->context[ "materialIndices" ]->set( optix.materialIndices );
		optixRenderer->context[ "materialInfos" ]->set( optix.materialInfos );
		optix.objects.material[ "textureInfos" ]->set( optix.textureInfos );
	}

	// set up the terrain buffers
	{
		optix.terrain.material = optixRenderer->context->createMaterial ();
		optix.terrain.material->setClosestHitProgram (0, optix.terrain.closestHit);
		optix.terrain.material->setAnyHitProgram (1, optix.terrain.anyHit);
		optix.terrain.material->validate ();

		int primitiveCount = scene->terrain.indices.size() / 3;
		optix.terrain.indexBuffer = optixRenderer->context->createBufferFromGLBO( RT_BUFFER_INPUT, terrainIndices.handle );
		optix.terrain.indexBuffer->setFormat( RT_FORMAT_UNSIGNED_INT3 );
		optix.terrain.indexBuffer->setSize( primitiveCount );
		optix.terrain.indexBuffer->validate();

		optix.terrain.vertexBuffer = optixRenderer->context->createBufferFromGLBO( RT_BUFFER_INPUT, terrainVertices.handle );
		optix.terrain.vertexBuffer->setSize( scene->terrain.vertices.size() );
		optix.terrain.vertexBuffer->setFormat( RT_FORMAT_USER );
		optix.terrain.vertexBuffer->setElementSize( sizeof SGSScene::Terrain::Vertex );
		optix.terrain.vertexBuffer->validate();

		optix.terrain.geometry = optixRenderer->context->createGeometry ();
		optix.terrain.geometry ["vertexBuffer"]->setBuffer (optix.terrain.vertexBuffer);
		optix.terrain.geometry ["indexBuffer"]->setBuffer (optix.terrain.indexBuffer);
		optix.terrain.geometry->setBoundingBoxProgram (optix.terrain.boundingBox);
		optix.terrain.geometry->setIntersectionProgram (optix.terrain.intersect);

		optix.terrain.geometry->setPrimitiveCount ( primitiveCount );
		optix.terrain.geometry->validate ();

		optix.terrainTextureSampler = optixRenderer->context->createTextureSamplerFromGLImage( bakedTerrainTexture.handle, RT_TARGET_GL_TEXTURE_2D );
		optix.terrainTextureSampler->setWrapMode( 0, RT_WRAP_REPEAT );
		optix.terrainTextureSampler->setWrapMode( 1, RT_WRAP_REPEAT );
		optix.terrainTextureSampler->setIndexingMode( RT_TEXTURE_INDEX_NORMALIZED_COORDINATES );
		optix.terrainTextureSampler->setReadMode( RT_TEXTURE_READ_NORMALIZED_FLOAT );
		optix.terrainTextureSampler->setMaxAnisotropy( 1.0f );
		optix.terrainTextureSampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

		optix.terrain.material[ "terrainTexture" ]->setTextureSampler( optix.terrainTextureSampler );

		optix.terrain.geometryInstance = optixRenderer->context->createGeometryInstance (optix.terrain.geometry, &optix.terrain.material, &optix.terrain.material + 1);
		optix.terrain.geometryInstance->validate ();
	}

	optix.staticAcceleration = optixRenderer->context->createAcceleration ("Sbvh", "Bvh");

	BOOST_STATIC_ASSERT( sizeof( SGSScene::Vertex ) == sizeof( SGSScene::Terrain::Vertex) );
	/*optix.staticAcceleration->setProperty( "vertex_buffer_name", "vertexBuffer" );
	optix.staticAcceleration->setProperty( "vertex_buffer_stride", boost::lexical_cast<std::string>( sizeof( SGSScene::Vertex ) ) );
	optix.staticAcceleration->setProperty( "index_buffer_name", "indexBuffer" );
	*/
	optix.staticAcceleration->validate ();

	optix.staticScene = optixRenderer->context->createGeometryGroup();
	optix.staticScene->setAcceleration (optix.staticAcceleration);
	optix.staticScene->setChildCount (2);
	optix.staticScene->setChild (0, optix.objects.geometryInstance);
	optix.staticScene->setChild (1, optix.terrain.geometryInstance);
	optix.staticScene->validate ();

	optixRenderer->addSceneChild( optix.staticScene );

	// cache
	{
		Optix::Cache cache;
		bool cacheChanged = false;

		const char *optixCacheFilename = "scene.optixCache";
		{
			boost::timer::auto_cpu_timer timer( "initOptix; load cache: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

			Serializer::BinaryReader reader( optixCacheFilename, sizeof Optix::Cache );
			if( reader.valid() ) {
				reader.get( cache );
			}

			if( cache.magicStamp != scene->numSceneVertices ) {
				cache = Optix::Cache();

				cache.magicStamp = scene->numSceneVertices;
			}
		}

		if( cache.staticSceneAccelerationCache.empty() ) {
			boost::timer::auto_cpu_timer timer( "initOptix; build acceleration structure: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

			// build the static scene acceleration tree
			optixRenderer->context->launch( 0, 0 );

			int size;
			size = optix.staticAcceleration->getDataSize();
			cache.staticSceneAccelerationCache.resize( size );
			optix.staticAcceleration->getData( &cache.staticSceneAccelerationCache.front() );

			cacheChanged = true;
		}
		else {
			// load from the cache
			optix.staticAcceleration->setData( &cache.staticSceneAccelerationCache.front(), cache.staticSceneAccelerationCache.size() );
		}

		// dump the cache?
		if( cacheChanged ) {
			boost::timer::auto_cpu_timer timer( "initOptix; write cache: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );
			
			Serializer::BinaryWriter writer( optixCacheFilename, sizeof Optix::Cache );
			writer.put( cache );
		}
	}
}

void OptixRenderer::init( const std::shared_ptr< SGSSceneRenderer > &sgsSceneRenderer ) {
	this->sgsSceneRenderer = sgsSceneRenderer;

	context = optix::Context::create();

	// initialize the context
	context->setRayTypeCount( OptixProgramInterface::RT_COUNT );
	context->setEntryPointCount(2);
	context->setCPUNumThreads( 12 );
	context->setStackSize(8000);
	context->setPrintBufferSize(65536);
	context->setPrintEnabled(true);

	// create the rt programs
	const char *raytracerPtxFilename = "cuda_compile_ptx_generated_raytracer.cu.ptx";
	programs.raytracer_exception = context->createProgramFromPTXFile (raytracerPtxFilename, "exception");
	programs.miss = context->createProgramFromPTXFile (raytracerPtxFilename, "miss");

	const char *probeSamplerPtxFilename = "cuda_compile_ptx_generated_probeTracer.cu.ptx";
	programs.probeSampler_exception = context->createProgramFromPTXFile (probeSamplerPtxFilename, "exception");

	// set the rt programs
	context->setMissProgram( 0, programs.miss );
	context->setExceptionProgram( 0, programs.raytracer_exception );
	context->setExceptionProgram( 1, programs.probeSampler_exception );
	
	// set the pinhole camera entry point
	context->setRayGenerationProgram(  0, context->createProgramFromPTXFile( raytracerPtxFilename, "pinholeCamera_rayGeneration" ) );
	// set the probe sampler entry point
	context->setRayGenerationProgram( 1, context->createProgramFromPTXFile( probeSamplerPtxFilename, "sampleProbes" ) );

	// create the acceleration structure
	acceleration = context->createAcceleration ("Bvh", "Bvh");

	// create the scene node
	scene = context->createGroup ();
	scene->setAcceleration (acceleration);

	context["rootObject"]->set(scene);

	// create and set the output buffer
	outputBuffer = context->createBuffer( RT_BUFFER_OUTPUT,  RT_FORMAT_UNSIGNED_BYTE4, width = 800, height = 600 );	
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

	// init the output texture
	SimpleGL::setLinearMinMag( debugTexture );

	// init our main object source
	sgsSceneRenderer->initOptix( this );

	// validate all objects
	scene->validate ();
	acceleration->validate();
	context->validate();

	auto r = rtContextCompile (context->get());
	const char* e;
	rtContextGetErrorString (context->get(), r, &e);
	std::cout << e;
}

void OptixRenderer::renderPinholeCamera(const Eigen::Matrix4f &projectionView, const Eigen::Vector3f &worldViewerPosition) {
	context[ "eyePosition" ]->set3fv( worldViewerPosition.data() );

	// this works with all usual projection matrices (where x and y don't have any effect on z and w in clip space)
	// determine u, v, and w by unprojecting (x,y,-1,1) from clip space to world space
	Eigen::Matrix4f inverseProjectionView = projectionView.inverse();
	// this is the w coordinate of the unprojected coordinates
	const float unprojectedW = inverseProjectionView(3,3) - inverseProjectionView(3,2);

	// divide the homogeneous affine matrix by the projected w
	// see R1 page for deduction
	Eigen::Matrix< float, 3, 4> inverseProjectionView34 = projectionView.inverse().topLeftCorner<3,4>() / unprojectedW;
	const Eigen::Vector3f u = inverseProjectionView34.col(0);
	const Eigen::Vector3f v = inverseProjectionView34.col(1);
	const Eigen::Vector3f w = inverseProjectionView34.col(3) - inverseProjectionView34.col(2) - worldViewerPosition;

	context[ "U" ]->set3fv( u.data() );
	context[ "V" ]->set3fv( v.data() );
	context[ "W" ]->set3fv( w.data() );

	context->launch(0, width, height );

	void *data = outputBuffer->map();
	debugTexture.load( 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	//SOIL_save_image( "frame.bmp", SOIL_SAVE_TYPE_BMP, 640, 480, 4, (unsigned char*) data );
	outputBuffer->unmap();
}