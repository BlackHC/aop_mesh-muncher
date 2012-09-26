#include "sgsSceneRenderer.h"
#include "optixRenderer.h"
#include "optixProgramHelpers.h"
#include "boost/timer/timer.hpp"

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
		::memcpy( optix.textureInfos->map(), &mergedTextureInfos.front(), sizeof( MergedTextureInfo ) * mergedTextureInfos.size() );
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

	// prepare the dynamic buffers
	{
		optix.dynamicObjects.indexBuffer = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_INT3, 1 );
		optix.dynamicObjects.indexBuffer->validate();

		optix.dynamicObjects.vertexBuffer = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, 1 );
		optix.dynamicObjects.vertexBuffer->setElementSize( sizeof SGSScene::Vertex );
		optix.dynamicObjects.vertexBuffer->validate();

		optix.dynamicObjects.geometry = optixRenderer->context->createGeometry ();
		OptixHelpers::Namespace::setGeometryPrograms( optix.dynamicObjects.geometry, objectModule, "" );
		optix.dynamicObjects.geometry->setPrimitiveCount ( 0 );
		optix.dynamicObjects.geometry->validate ();

		optix.dynamicObjects.geometryInstance = optixRenderer->context->createGeometryInstance (optix.dynamicObjects.geometry, &optix.objectMaterial, &optix.objectMaterial + 1);
		optix.dynamicObjects.geometryInstance->validate ();

		optix.dynamicObjects.geometryInstance[ "vertexBuffer" ]->setBuffer( optix.dynamicObjects.vertexBuffer );
		optix.dynamicObjects.geometryInstance[ "indexBuffer" ]->setBuffer( optix.dynamicObjects.indexBuffer );

		// set materialInfos and materialIndices
		optix.dynamicObjects.materialInfos = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_USER, 0 );
		optix.dynamicObjects.materialInfos->setElementSize( sizeof OptixProgramInterface::MaterialInfo );

		optix.dynamicObjects.materialIndices = optixRenderer->context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT, 0 );

#if 0
		OptixProgramInterface::MaterialInfo *materialInfos = (OptixProgramInterface::MaterialInfo *) optix.dynamicObjects.materialInfos->map();	
		int *materialIndices = (int *) optix.dynamicObjects.materialIndices->map();

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

				const int startPrimitive = subObject.startIndex / 3;
				const int endPrimitive = startPrimitive + subObject.numIndices / 3;
				std::fill( materialIndices + startPrimitive, materialIndices + endPrimitive, subObjectIndex );
			}
		}
		optix.dynamicObjects.materialIndices->unmap();
		optix.dynamicObjects.materialInfos->unmap();
#endif
		optix.dynamicObjects.materialIndices->validate();
		optix.dynamicObjects.materialInfos->validate();

		optix.dynamicObjects.geometryInstance[ "materialIndices" ]->set( optix.dynamicObjects.materialIndices );
		optix.dynamicObjects.geometryInstance[ "materialInfos" ]->set( optix.dynamicObjects.materialInfos );
	}

	// set up the terrain buffers
	{
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
	optix.staticScene->setChildCount (2);
	optix.staticScene->setChild (0, optix.staticObjects.geometryInstance);
	optix.staticScene->setChild (1, optix.terrain.geometryInstance);
	optix.staticScene->validate ();

	optix.dynamicScene = optixRenderer->context->createGeometryGroup();
	optix.dynamicScene->setAcceleration( optix.dynamicAcceleration );
	optix.dynamicScene->setChildCount( 0 );
	optix.dynamicScene->validate();

	optixRenderer->addSceneChild( optix.staticScene );
	optixRenderer->addSceneChild( optix.dynamicScene );

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

void SGSSceneRenderer::refillDynamicOptixBuffers() {
	int numVertices = 0;
	int numIndices = 0;
	int numSubObjects = 0;

	for( int instanceIndex = 0 ; instanceIndex < instances.size() ; instanceIndex++ ) {
		const SGSScene::Model &model = scene->models[ instances[ instanceIndex ].modelId ];
		
		numSubObjects += model.numSubObjects;
		const int endSubObject = model.startSubObject + model.numSubObjects;
		for( int subObjectIndex = model.startSubObject ; subObjectIndex < endSubObject ; subObjectIndex++ ) {
			const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];
			numVertices += subObject.numVertices;
			numIndices += subObject.numIndices;
		}
	}

	optix.dynamicObjects.indexBuffer->setSize( numIndices );
	optix.dynamicObjects.indexBuffer->validate();

	optix.dynamicObjects.vertexBuffer->setSize( numVertices );
	optix.dynamicObjects.vertexBuffer->validate();

	optix.dynamicObjects.geometry->setPrimitiveCount ( numIndices / 3 );

	optix.dynamicObjects.materialInfos->setSize( numSubObjects );
	optix.dynamicObjects.materialIndices->setSize( numVertices );

	auto materialInfos = (OptixProgramInterface::MaterialInfo *) optix.dynamicObjects.materialInfos->map();	
	auto materialIndices = (int *) optix.dynamicObjects.materialIndices->map();
	auto vertices = (SGSScene::Vertex *) optix.dynamicObjects.vertexBuffer->map();
	auto indices = (unsigned int *) optix.dynamicObjects.indexBuffer->map();
	
	int globalVertexIndex = 0;
	int globalIndexIndex = 0;
	int globalSubObjectIndex = 0;

	for( int instanceIndex = 0 ; instanceIndex < instances.size() ; instanceIndex++ ) {
		const Instance &instance = instances[ instanceIndex ];
		const SGSScene::Model &model = scene->models[ instance.modelId ];

		Eigen::Matrix3f normalTransformation = instance.transformation.topLeftCorner<3,3>().inverse().transpose();

		numSubObjects += model.numSubObjects;
		const int endSubObject = model.startSubObject + model.numSubObjects;
		for( int subObjectIndex = model.startSubObject ; subObjectIndex < endSubObject ; ++subObjectIndex ) {
			const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

			// set the material info
			OptixProgramInterface::MaterialInfo &materialInfo = materialInfos[ globalSubObjectIndex ];
			materialInfo.modelIndex = instance.modelId;
			materialInfo.objectIndex = instanceIndex + scene->objects.size();

			materialInfo.textureIndex = subObject.material.textureIndex[0];
			materialInfo.alphaType = (OptixProgramInterface::MaterialInfo::AlphaType) subObject.material.alphaType;
			materialInfo.alpha = subObject.material.alpha / 255.0f;

			// set the material indices
			const int startPrimitive = globalIndexIndex / 3;
			const int endPrimitive = globalIndexIndex + subObject.numIndices / 3;
			std::fill( materialIndices + startPrimitive, materialIndices + endPrimitive, globalSubObjectIndex );

			int startVertex = globalVertexIndex;

			// copy vertices
			for( int vertexIndex = subObject.startVertex ; vertexIndex < subObject.startVertex + subObject.numVertices ; ++vertexIndex ) {
				auto &modelVertex = scene->vertices[ vertexIndex ];
				auto &instanceVertex = vertices[ globalVertexIndex++ ];

				instanceVertex = modelVertex;
				Eigen::Vector3f::Map( instanceVertex.position ) = (instance.transformation * Eigen::Vector3f::Map( instanceVertex.position ).homogeneous()).eval().hnormalized();
				Eigen::Vector3f::Map( instanceVertex.normal ) = normalTransformation * Eigen::Vector3f::Map( instanceVertex.normal );
			}

			// copy indices
			int indexShift = startVertex - subObject.startVertex;
			for( int indexIndex = subObject.startIndex ; indexIndex < subObject.startIndex + subObject.numIndices ; ++indexIndex ) {
				indices[ globalIndexIndex++ ] = scene->indices[ indexIndex ] + indexShift;
			}

			++globalSubObjectIndex;
		}
	}
	optix.dynamicObjects.materialIndices->unmap();
	optix.dynamicObjects.materialInfos->unmap();
	optix.dynamicObjects.vertexBuffer->unmap();
	optix.dynamicObjects.indexBuffer->unmap();

	optix.dynamicScene->setChildCount( 1 );
	optix.dynamicScene->setChild( 0, optix.dynamicObjects.geometryInstance );

	optix.dynamicAcceleration->markDirty();
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

	// init our main object source
	sgsSceneRenderer->initOptix( this );

	// validate all objects
	scene->validate ();
	acceleration->validate();
	context->validate();

	compileContext();
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
	setRenderContext( renderContext );
	setPinholeCameraViewerContext( viewerContext );

	context->launch( OptixProgramInterface::renderPinholeCameraView, width, height );

	void *data = outputBuffer->map();
	debugTexture.load( 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	//SOIL_save_image( "frame.bmp", SOIL_SAVE_TYPE_BMP, 640, 480, 4, (unsigned char*) data );
	outputBuffer->unmap();
}

void OptixRenderer::selectFromPinholeCamera( const std::vector< optix::float2 > &selectionRays, std::vector< OptixProgramInterface::SelectionResult > &selectionResults, const ViewerContext &viewerContext, const RenderContext &renderContext ) {
	setRenderContext( renderContext );
	setPinholeCameraViewerContext( viewerContext );

	OptixHelpers::Buffer::copyToDevice( this->selectionRays, selectionRays );
	
	context->launch( OptixProgramInterface::selectFromPinholeCamera, selectionRays.size() );

	selectionResults.resize( selectionRays.size() );
	OptixHelpers::Buffer::copyToHost( this->selectionResults, selectionResults.front(), selectionResults.size() );
}

void OptixRenderer::sampleProbes( const std::vector< Probe > &probes, std::vector< ProbeContext > &probeContexts, const RenderContext &renderContext, float maxDistance ) {
	setRenderContext( renderContext );

	context[ "maxDistance" ]->setFloat( maxDistance );

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
}