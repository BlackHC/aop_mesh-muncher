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

DebugRender::CombinedCalls selectionDR;

void selectObjectsByModelID( SGSSceneRenderer &renderer, int modelIndex ) {
	SGSSceneRenderer::InstanceIndices indices = renderer.getModelInstances( modelIndex );

	selectionDR.begin();
	selectionDR.setColor( Eigen::Vector3f::UnitX() );

	for( auto instanceIndex = indices.begin() ; instanceIndex != indices.end() ; ++instanceIndex ) {
		auto transformation = renderer.getInstanceTransformation( *instanceIndex );
		auto boundingBox = renderer.getUntransformedInstanceBoundingBox( *instanceIndex );

		selectionDR.setTransformation( transformation.matrix() );	
		selectionDR.drawAABB( boundingBox.min(), boundingBox.max() );
	}

	selectionDR.end();
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
	cameraInputControl.init( make_nonallocated_shared(camera) );

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

	EventSystem eventSystem;
	EventDispatcher eventDispatcher;
	eventSystem.setRootHandler( make_nonallocated_shared( eventDispatcher ) );
	eventSystem.exclusiveMode.window = make_nonallocated_shared( window );
	eventDispatcher.addEventHandler( make_nonallocated_shared( cameraInputControl ) );

	EventDispatcher verboseEventDispatcher;
	eventDispatcher.addEventHandler( make_nonallocated_shared( verboseEventDispatcher ) );

	registerConsoleHelpAction( verboseEventDispatcher );

	KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { sgsSceneRenderer.reloadShaders(); } );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( reloadShadersAction ) );

	BoolVariableToggle showBoundingSpheresToggle( "show bounding spheres",sgsSceneRenderer.debug.showBoundingSpheres, sf::Keyboard::B );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showBoundingSpheresToggle ) );

	BoolVariableToggle showTerrainBoundingSpheresToggle( "show terrain bounding spheres",sgsSceneRenderer.debug.showTerrainBoundingSpheres, sf::Keyboard::N );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showTerrainBoundingSpheresToggle ) );

	BoolVariableToggle updateRenderListsToggle( "updateRenderLists",sgsSceneRenderer.debug.updateRenderLists, sf::Keyboard::C );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( updateRenderListsToggle ) );

	IntVariableControl disabledObjectIndexControl( "disabledModelIndex", renderContext.disabledModelIndex, -1, sgsScene.modelNames.size(), sf::Keyboard::Numpad7, sf::Keyboard::Numpad1 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledObjectIndexControl ) );

	IntVariableControl disabledInstanceIndexControl( "disabledInstanceIndex",renderContext.disabledInstanceIndex, -1, sgsScene.numSceneObjects, sf::Keyboard::Numpad9, sf::Keyboard::Numpad3 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledInstanceIndexControl ) );

	
	DebugRender::CombinedCalls probeDumps;
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
		//glColor4ubv( &probeContexts.front().color.x );
		probeDumps.drawVectorCone( probeContexts.front().distance * direction, probeContexts.front().distance * 0.25, 1 + probeContexts.front().hitCounter );
		probeDumps.end();
	} );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( dumpProbeAction ) );

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
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disableObjectAction ) );

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
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disableInstanceAction ) );

	DebugWindowManager debugWindowManager;
	
#if 1
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

			eventSystem.processEvent( event );
		}

		if( !window.isOpen() ) {
			break;
		}

		eventSystem.update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );
		
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

			sgsSceneRenderer.renderScene( camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition(), renderContext );

			probeDumps.render();		

			selectObjectsByModelID( sgsSceneRenderer, renderContext.disabledModelIndex );
			glDisable( GL_DEPTH_TEST );
			selectionDR.render();
			glEnable( GL_DEPTH_TEST );
			
			const ViewerContext viewerContext = { camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() };
			optixRenderer.renderPinholeCamera( viewerContext, renderContext );

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