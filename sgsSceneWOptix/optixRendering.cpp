#define NOMINMAX
#include <Windows.h>
#undef near
#undef far

#include "sgsSceneRender.h"

void OptixRenderer::init( const std::shared_ptr< SGSSceneRenderer > &sgsSceneRenderer ) {
	this->sgsSceneRenderer = sgsSceneRenderer;

	context = optix::Context::create();

	// initialize the context
	context->setRayTypeCount( OptixProgramInterface::RT_COUNT );
	context->setEntryPointCount(1);
	context->setStackSize(8000);
	context->setPrintBufferSize(65536);
	context->setPrintEnabled(true);

	// create the rt programs
	const char *raytracerPtxFilename = "cuda_compile_ptx_generated_raytracer.cu.ptx";
	programs.exception = context->createProgramFromPTXFile (raytracerPtxFilename, "exception");
	programs.miss = context->createProgramFromPTXFile (raytracerPtxFilename, "miss");

	// set the rt programs
	context->setMissProgram( 0, programs.miss );
	context->setExceptionProgram( 0, programs.exception );
	
	// set the pinhole camera entry point
	context->setRayGenerationProgram (0, context->createProgramFromPTXFile (raytracerPtxFilename, "pinholeCamera_rayGeneration"));

	// create the acceleration structure
	acceleration = context->createAcceleration ("Bvh", "Bvh");

	// create the scene node
	scene = context->createGroup ();
	scene->setAcceleration (acceleration);

	context["rootObject"]->set(scene);

	// create and set the output buffer
	outputBuffer = context->createBuffer( RT_BUFFER_OUTPUT,  RT_FORMAT_UNSIGNED_BYTE4, width = 640, height = 480 );	
	context["outputBuffer"]->set(outputBuffer);

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