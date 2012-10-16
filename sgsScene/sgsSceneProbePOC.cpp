#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <boost/timer/timer.hpp>
#include <iterator>

#include <Eigen/Eigen>
#include <GL/glew.h>
#include <unsupported/Eigen/OpenGLSupport>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Debug.h"

#include <memory>

using namespace Eigen;

#include "camera.h"
#include "cameraInputControl.h"
#include <verboseEventHandlers.h>

#include "make_nonallocated_shared.h"

#include "sgsSceneRenderer.h"
#include "optixRenderer.h"

#include "debugWindows.h"
#include "grid.h"

DebugRender::CombinedCalls selectionDR;

void selectObjectsByModelID( SGSSceneRenderer &renderer, int modelIndex ) {
	SGSSceneRenderer::InstanceIndices indices = renderer.getModelInstances( modelIndex );

	selectionDR.begin();
	selectionDR.setColor( Eigen::Vector3f::UnitX() );

	for( auto instanceIndex = indices.begin() ; instanceIndex != indices.end() ; ++instanceIndex ) {
		auto transformation = renderer.getInstanceTransformation( *instanceIndex );
		auto boundingBox = renderer.getUntransformedInstanceBoundingBox( *instanceIndex );

		selectionDR.setTransformation( transformation );	
		selectionDR.drawAABB( boundingBox.min(), boundingBox.max() );
	}

	selectionDR.end();
}

DebugRender::CombinedCalls probeDumps;

void sampleInstances( OptixRenderer &optix, SGSSceneRenderer &renderer, int modelIndex ) {
	SGSSceneRenderer::InstanceIndices indices = renderer.getModelInstances( modelIndex );

	std::vector< OptixRenderer::ProbeContext > probeContexts;
	std::vector< OptixRenderer::Probe > probes;

	for( int instanceIndexIndex = 0 ; instanceIndexIndex < indices.size() ; ++instanceIndexIndex ) {
		const auto &instanceIndex = indices[ instanceIndexIndex ];
		
		RenderContext renderContext;
		renderContext.disabledModelIndex = -1;
		renderContext.disabledInstanceIndex = instanceIndex;

		// create probes
		std::vector< OptixRenderer::Probe > localProbes;

		const float resolution = 1;
		auto boundingBox = renderer.getUntransformedInstanceBoundingBox( instanceIndex );
		auto transformation = Eigen::Affine3f( renderer.getInstanceTransformation( instanceIndex ) );
		auto mapping = createIndexMapping( ceil( boundingBox.sizes() / resolution ),boundingBox.min(), resolution );

		auto center = transformation * boundingBox.center();
		for( auto iterator = mapping.getIterator() ; iterator.hasMore() ; ++iterator ) {
			OptixRenderer::Probe probe;
			auto probePosition = Eigen::Vector3f::Map( &probe.position.x );
			auto probeDirection = Eigen::Vector3f::Map( &probe.direction.x );
			probePosition = transformation * mapping.getPosition( iterator.getIndex3() );
			probeDirection = (probePosition - center).normalized();
			localProbes.push_back( probe );
		}

		std::vector< OptixRenderer::ProbeContext > localProbeContexts;
		optix.sampleProbes( localProbes, localProbeContexts, renderContext );

		boost::push_back( probes, localProbes );
		boost::push_back( probeContexts, localProbeContexts );
	}

	probeDumps.begin();
	for( int probeContextIndex = 0 ; probeContextIndex < probeContexts.size() ; ++probeContextIndex ) {
		const auto &probeContext = probeContexts[ probeContextIndex ];
		probeDumps.setPosition( Eigen::Vector3f::Map( &probes[ probeContextIndex ].position.x ) );
		glColor4ubv( &probeContext.color.x );		
		probeDumps.drawVectorCone( probeContext.distance * Eigen::Vector3f::Map( &probes[ probeContextIndex ].direction.x ), probeContexts.front().distance * 0.25, 1 + probeContext.hitPercentage * 15 );	
	}
	probeDumps.end();
}

void real_main() {
	sf::RenderWindow window( sf::VideoMode( 640, 480 ), "sgsSceneViewer", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
	glewInit();

	glutil::RegisterDebugOutput( glutil::STD_OUT );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 0.05;
	camera.perspectiveProjectionParameters.zFar = 500.0;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( make_nonallocated_shared(camera), make_nonallocated_shared(window) );

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	SGSSceneRenderer sgsSceneRenderer;
	OptixRenderer optixRenderer;
	SGSScene sgsScene;
	RenderContext renderContext;
	renderContext.setDefault();

	{
		boost::timer::auto_cpu_timer timer( "SGSSceneRenderer: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		sgsSceneRenderer.reloadShaders();

		const char *scenePath = "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene";
		{
			Serializer::BinaryReader reader( scenePath );
			Serializer::read( reader, sgsScene );
		}
		
		const char *cachePath = "scene.sgsRendererCache";
		sgsSceneRenderer.processScene( make_nonallocated_shared( sgsScene ), cachePath );
	}
	{
		boost::timer::auto_cpu_timer timer( "OptixRenderer: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		optixRenderer.init( make_nonallocated_shared( sgsSceneRenderer ) );
	}

	EventDispatcher eventDispatcher;
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( cameraInputControl ) );

	VerboseEventDispatcher verboseEventDispatcher;
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( verboseEventDispatcher ) );

	verboseEventDispatcher.registerConsoleHelpAction();

	KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { sgsSceneRenderer.reloadShaders(); } );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( reloadShadersAction ) );

	BoolVariableToggle showBoundingSpheresToggle( "show bounding spheres",sgsSceneRenderer.debug.showBoundingSpheres, sf::Keyboard::B );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( showBoundingSpheresToggle ) );

	BoolVariableToggle showTerrainBoundingSpheresToggle( "show terrain bounding spheres",sgsSceneRenderer.debug.showTerrainBoundingSpheres, sf::Keyboard::N );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( showTerrainBoundingSpheresToggle ) );

	BoolVariableToggle updateRenderListsToggle( "updateRenderLists",sgsSceneRenderer.debug.updateRenderLists, sf::Keyboard::C );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( updateRenderListsToggle ) );

	IntVariableControl disabledObjectIndexControl( "disabledModelIndex", renderContext.disabledModelIndex, -1, sgsScene.modelNames.size(), sf::Keyboard::Numpad7, sf::Keyboard::Numpad1 );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( disabledObjectIndexControl ) );

	IntVariableControl disabledInstanceIndexControl( "disabledInstanceIndex",renderContext.disabledInstanceIndex, -1, sgsScene.numSceneObjects, sf::Keyboard::Numpad9, sf::Keyboard::Numpad3 );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( disabledInstanceIndexControl ) );

	KeyAction probeInstances( "probe disabled instances", sf::Keyboard::X, [&] () {
			sampleInstances( optixRenderer, sgsSceneRenderer, renderContext.disabledModelIndex );
		});
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( probeInstances ) );

	
	KeyAction dumpProbeAction( "dump probe", sf::Keyboard::P, [&] () { 
		// dump a probe at the current position and view direction
		const Eigen::Vector3f position = camera.getPosition();
		const Eigen::Vector3f direction = camera.getDirection();

		std::vector< OptixRenderer::Probe > probes;
		std::vector< OptixRenderer::ProbeContext > probeContexts;
		
		OptixRenderer::Probe probe;
		Eigen::Vector3f::Map( &probe.position.x ) = position;
		Eigen::Vector3f::Map( &probe.direction.x ) = direction;
		
		probes.push_back( probe );

		optixRenderer.sampleProbes( probes, probeContexts, renderContext );

		probeDumps.append();
		probeDumps.setPosition( position );
		glColor4ubv( &probeContexts.front().color.x );		
		probeDumps.drawVectorCone( probeContexts.front().distance * direction, probeContexts.front().distance * 0.25, 1 + probeContexts.front().hitPercentage * 15 );
		probeDumps.end();
	} );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( dumpProbeAction ) );

	KeyAction disableObjectAction( "disable models shot", sf::Keyboard::Numpad4, [&] () { 
		// dump a probe at the current position and view direction
		const ViewerContext viewerContext = { camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() };

		OptixRenderer::SelectionRays selectionRays;
		selectionRays.push_back( optix::make_float2( 0.0f ) );

		OptixRenderer::SelectionResults selectionResults;
		optixRenderer.selectFromPinholeCamera( selectionRays, selectionResults, viewerContext, renderContext );
	
		renderContext.disabledModelIndex = selectionResults.front().modelIndex;
		std::cout << "object: " << selectionResults.front().modelIndex << "\n";
	} );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( disableObjectAction ) );

	KeyAction disableInstanceAction( "disable instance shot", sf::Keyboard::Numpad6, [&] () { 
		// dump a probe at the current position and view direction
		const ViewerContext viewerContext = { camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() };

		OptixRenderer::SelectionRays selectionRays;
		selectionRays.push_back( optix::make_float2( 0.0f ) );

		OptixRenderer::SelectionResults selectionResults;
		optixRenderer.selectFromPinholeCamera( selectionRays, selectionResults, viewerContext, renderContext );

		renderContext.disabledInstanceIndex = selectionResults.front().objectIndex;
		std::cout << "instance: " << selectionResults.front().objectIndex << "\n";
	} );
	verboseEventDispatcher.eventHandlers.push_back( make_nonallocated_shared( disableInstanceAction ) );

	DebugWindowManager debugWindowManager;
	
#if 0
	TextureVisualizationWindow optixWindow;
	optixWindow.init( "Optix Version" );
	optixWindow.texture = optixRenderer.debugTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( optixWindow ) );
#endif
#if 0
	TextureVisualizationWindow mergedTextureWindow;
	mergedTextureWindow.init( "merged object textures" );
	mergedTextureWindow.texture = sgsSceneRenderer.mergedTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( mergedTextureWindow ) );
#endif

	sf::Text renderDuration;
	renderDuration.setPosition( 0, 0 );
	renderDuration.setCharacterSize( 10 );

	Instance testInstance;
	testInstance.modelId = 1;
	testInstance.transformation.setIdentity();
	sgsSceneRenderer.addInstance( testInstance );

	while (true)
	{
		// Activate the window for OpenGL rendering
		window.setActive();

		// Event processing
		sf::Event event;
		while (window.pollEvent(event))
		{
			// Request for closing the window
			if (event.type == sf::Event::Closed)
				window.close();

			if( event.type == sf::Event::Resized ) {
				camera.perspectiveProjectionParameters.aspect = float( event.size.width ) / event.size.height;
				glViewport( 0, 0, event.size.width, event.size.height );
			}

			eventDispatcher.handleEvent( event );
		}

		if( !window.isOpen() ) {
			break;
		}

		cameraInputControl.update( frameClock.restart().asSeconds(), false );
		
		{
			boost::timer::cpu_timer renderTimer;
			sgsSceneRenderer.renderShadowmap( renderContext );

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// OpenGL drawing commands go here...
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( camera.getProjectionMatrix() );
			glMultMatrix( camera.getViewTransformation().matrix() );

			glMatrixMode( GL_MODELVIEW );
			// TODO: move this into the render functions [9/23/2012 kirschan2]
			glLoadIdentity();

			sgsSceneRenderer.renderSceneView( camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition(), renderContext );

			probeDumps.render();		

			selectObjectsByModelID( sgsSceneRenderer, renderContext.disabledModelIndex );
			glDisable( GL_DEPTH_TEST );
			selectionDR.render();
			glEnable( GL_DEPTH_TEST );
			
			const ViewerContext viewerContext = { camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() };
			//optixRenderer.renderPinholeCamera( viewerContext, renderContext );

			// End the current frame and display its contents on screen
			renderDuration.setString( renderTimer.format() );
			window.pushGLStates();
			window.resetGLStates();
			window.draw( renderDuration );
			window.popGLStates();
			window.display();

			debugWindowManager.update();
		}
		
	}
};

void main() {
	try {
		real_main();
	}
	catch( std::exception &e) {
		std::cout << e.what() << "\n";
	}
}