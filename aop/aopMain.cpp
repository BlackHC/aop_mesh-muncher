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

#include "sgsInterface.h"
#include "mathUtility.h"
#include "optixEigenInterop.h"
#include "grid.h"
#include "probeGenerator.h"
#include "mathUtility.h"

#include "editor.h"

//#include "candidateFinderInterface.h"

void visualizeProbes( float resolution, const std::vector< SGSInterface::Probe > &probes );

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
		probeDumps.drawVectorCone( probeContext.distance * Eigen::Vector3f::Map( &probes[ probeContextIndex ].direction.x ), probeContexts.front().distance * 0.25, 1 + float( probeContext.hitCounter ) / OptixProgramInterface::numProbeSamples * 15 );
	}
	probeDumps.end();
}

#if 1

struct MouseDelta {
	sf::Vector2i lastPosition;

	void reset() {
		lastPosition = sf::Mouse::getPosition();
	}

	sf::Vector2i pop() {
		const sf::Vector2i currentPosition = sf::Mouse::getPosition();
		const sf::Vector2i delta = currentPosition - lastPosition;
		lastPosition = currentPosition;
		return delta;
	}
};


#endif

EventSystem *EventHandler::eventSystem;

void real_main() {
	sf::RenderWindow window( sf::VideoMode( 640, 480 ), "AOP", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
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

	SGSInterface::World world;
	SGSInterface::View view;
	view.renderContext.setDefault();

	const char *scenePath = "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene";
	world.init( scenePath );

	EventDispatcher eventDispatcher( "Root:" );
	eventDispatcher.addEventHandler( make_nonallocated_shared( cameraInputControl ) );

	registerConsoleHelpAction( eventDispatcher );

	EventSystem eventSystem;
	eventSystem.rootHandler = make_nonallocated_shared( eventDispatcher );
	eventSystem.exclusiveMode.window = make_nonallocated_shared( window );

	EventDispatcher verboseEventDispatcher( "sub" );
	eventDispatcher.addEventHandler( make_nonallocated_shared( verboseEventDispatcher ) );

	Editor editor;
	editor.world = &world;
	editor.view = &view;
	editor.init();

	eventDispatcher.addEventHandler( make_nonallocated_shared( editor ) );

	KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { world.sceneRenderer.reloadShaders(); } );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( reloadShadersAction ) );

	BoolVariableToggle showBoundingSpheresToggle( "show bounding spheres", world.sceneRenderer.debug.showBoundingSpheres, sf::Keyboard::B );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showBoundingSpheresToggle ) );

	BoolVariableToggle showTerrainBoundingSpheresToggle( "show terrain bounding spheres",world.sceneRenderer.debug.showTerrainBoundingSpheres, sf::Keyboard::N );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showTerrainBoundingSpheresToggle ) );

	BoolVariableToggle updateRenderListsToggle( "updateRenderLists",world.sceneRenderer.debug.updateRenderLists, sf::Keyboard::C );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( updateRenderListsToggle ) );

	IntVariableControl disabledObjectIndexControl( "disabledModelIndex", view.renderContext.disabledModelIndex, -1, world.scene.modelNames.size(), sf::Keyboard::Numpad7, sf::Keyboard::Numpad1 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledObjectIndexControl ) );

	IntVariableControl disabledInstanceIndexControl( "disabledInstanceIndex",view.renderContext.disabledInstanceIndex, -1, world.scene.numSceneObjects, sf::Keyboard::Numpad9, sf::Keyboard::Numpad3 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledInstanceIndexControl ) );

	DebugWindowManager debugWindowManager;

#if 0
	TextureVisualizationWindow optixWindow;
	optixWindow.init( "Optix Version" );
	optixWindow.texture = world.optixRenderer.debugTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( optixWindow ) );
#endif
#if 0
	TextureVisualizationWindow mergedTextureWindow;
	mergedTextureWindow.init( "merged object textures" );
	mergedTextureWindow.texture = world.sceneRenderer.mergedTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( mergedTextureWindow ) );
#endif

	sf::Text renderDuration;
	renderDuration.setPosition( 0, 0 );
	renderDuration.setCharacterSize( 10 );

	Instance testInstance;
	testInstance.modelId = 0;
	testInstance.transformation.setIdentity();
	world.sceneRenderer.addInstance( testInstance );

	ProbeGenerator::initDirections();

#if 0
	renderContext.disabledModelIndex = 0;
	DebugRender::DisplayList probeVisualization;
	{
		auto instanceIndices = sgsSceneRenderer.getModelInstances( 0 );

		int totalCount = 0;

		for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
			boost::timer::auto_cpu_timer timer( "ProbeSampling, batch: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

			std::vector<SGSInterface::Probe> probes, transformedProbes;

			SGSInterface::generateProbes( *instanceIndex, 0.25, sgsSceneRenderer, probes, transformedProbes );

			std::vector< OptixRenderer::ProbeContext > probeContexts;

			std::cout << "sampling " << transformedProbes.size() << " probes in one batch:\n\t";
			//optixRenderer.sampleProbes( transformedProbes, probeContexts, renderContext );

			totalCount += transformedProbes.size();

			probeVisualization.beginCompileAndAppend();
			visualizeProbes( 0.25, transformedProbes );
			probeVisualization.endCompile();
		}

		std::cout << "num probes: " << totalCount << "\n";
	}
#endif

	view.renderContext.disabledModelIndex = 0;

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


			//probeVisualization.render();

			/*selectObjectsByModelID( world.sceneRenderer, view.renderContext.disabledModelIndex );
			glDisable( GL_DEPTH_TEST );
			selectionDR.render();
			glEnable( GL_DEPTH_TEST );*/

			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( view.viewerContext.projectionView );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			view.updateFromCamera( camera );
			world.renderViewFrame( view );
			editor.render();

			//world.renderOptixViewFrame( view );

			renderDuration.setString( renderTimer.format() );
			window.pushGLStates();
			window.resetGLStates();
			window.draw( renderDuration );
			window.popGLStates();

			// End the current frame and display its contents on screen
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