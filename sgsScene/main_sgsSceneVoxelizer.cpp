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

enum GridVisualizationMode {
	GVM_POSITION,
	GVM_HITS,
	GVM_NORMAL,
	GVM_MAX
};

void visualizeColorGrid( const VoxelizedModel::Voxels &grid, DebugRender::DisplayList &displayList, GridVisualizationMode gvm = GVM_POSITION ) {
	const float size = grid.getMapping().getResolution();
	
	displayList.beginCompile();

	DebugRender::begin();
	for( auto iterator = grid.getIterator() ; iterator.hasMore() ; ++iterator ) {
		const auto &normalHit = grid[ *iterator ];

		if( normalHit.a != 0 ) {
			DebugRender::setPosition( grid.getMapping().getPosition( iterator.getIndex3() ) );

			Eigen::Vector3f positionColor = iterator.getIndex3().cast<float>().cwiseQuotient( grid.getMapping().getSize().cast<float>() );

			switch( gvm ) {
			case GVM_POSITION:
				DebugRender::setColor( positionColor );
				break;
			case GVM_HITS:
				DebugRender::setColor( Vector3f::UnitY() * (0.5 + normalHit.a / 128.0) );
				break;
			case GVM_NORMAL:
				glColor3ubv( &normalHit.r );
				break;
			}
			
			DebugRender::drawBox( Vector3f::Constant( size ), false );
		}
	}
	DebugRender::end();

	displayList.endCompile();
}

void real_main() {
	sf::RenderWindow window( sf::VideoMode( 640, 480 ), "sgsSceneVoxelizer", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
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

	DebugRender::DisplayList renderedVoxels;
	int modelIndex = 23;
	float resolution = 0.25;
	GridVisualizationMode gvm = GVM_POSITION;

	auto updateVoxels = [&] () {
		auto voxels = sgsSceneRenderer.voxelizeModel( modelIndex, resolution );
		
		visualizeColorGrid( voxels, renderedVoxels, gvm );
	};

	updateVoxels();

	IntVariableControl gvmControl( "cycle gvm", (int &) gvm, 0, (int) GVM_MAX, sf::Keyboard::Comma, sf::Keyboard::Period, updateVoxels );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( gvmControl ) );

	IntVariableControl modelIndexControl( "modelIndex", modelIndex, 0, sgsScene.modelNames.size(), sf::Keyboard::Up, sf::Keyboard::Down, updateVoxels );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( modelIndexControl ) );

	KeyAction revoxelizeAction( "revoxelize", sf::Keyboard::U, [&] () { updateVoxels(); } );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( revoxelizeAction ) );

	FloatVariableControl resolutionControl( "resolution", resolution, 0.10, 2.0, sf::Keyboard::PageUp, sf::Keyboard::PageDown, updateVoxels );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( resolutionControl ) );
	
	bool renderVoxels = true;
	bool renderModel = false;

	BoolVariableToggle renderVoxelsToggle( "toggle renderVoxels", renderVoxels, sf::Keyboard::V );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( renderVoxelsToggle ) );
	BoolVariableToggle renderModelToggle( "toggle renderModel", renderModel, sf::Keyboard::C );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( renderModelToggle ) );

	KeyAction cycleModesAction( "cycle voxel/rasterization", sf::Keyboard::B, [&] () { renderModel = renderVoxels; renderVoxels = !renderVoxels; } );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( cycleModesAction ) );

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

			if( renderModel ) {
				sgsSceneRenderer.renderModel( camera.getPosition(), modelIndex );
			}
			if( renderVoxels ) {
				Program::useFixed();
				renderedVoxels.render();
			}

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