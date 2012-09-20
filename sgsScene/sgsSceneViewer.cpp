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

#include "Debug.h"

#include <memory>

using namespace Eigen;

#include "camera.h"
#include "cameraInputControl.h"

#include "make_nonallocated_shared.h"

#include "sgsSceneRender.h"

#include "debugWindows.h"

struct IntVariableControl : EventHandler {
	sf::Keyboard::Key upKey, downKey;
	int *variable;
	int min, max;

	IntVariableControl( int *variable, int min, int max, sf::Keyboard::Key upKey = sf::Keyboard::Up, sf::Keyboard::Key downKey = sf::Keyboard::Down )
		: variable( variable ), min( min ), max( max ), upKey( upKey ), downKey( downKey ) {
	}

	virtual bool handleEvent( const sf::Event &event ) 
	{
		switch( event.type ) {
		case sf::Event::KeyPressed:
			if( event.key.code == upKey ) {
				*variable = std::min( *variable + 1, max );
				return true;
			}
			else if( event.key.code == downKey ) {
				*variable = std::max( *variable - 1, min );
				return false;
			}
			break;
		}
		return false;
	}
};

struct KeyAction : EventHandler {
	sf::Keyboard::Key key;
	std::function<void()> action;

	KeyAction( sf::Keyboard::Key key, const std::function<void()> &action) : key( key ), action( action ) {}

	virtual bool handleEvent( const sf::Event &event ) {
		if( event.type == sf::Event::KeyPressed && event.key.code == key ) {
			action();
			return true;
		}
		return false;
	} 
};

struct BoolVariableControl : EventHandler {
	sf::Keyboard::Key toggleKey, downKey;
	bool &variable;

	BoolVariableControl( bool &variable, sf::Keyboard::Key toggleKey = sf::Keyboard::T )
		: variable( variable ), toggleKey( toggleKey ) {
	}

	virtual bool handleEvent( const sf::Event &event ) 
	{
		switch( event.type ) {
		case sf::Event::KeyPressed:
			if( event.key.code == toggleKey ) {
				variable = !variable;
				return true;
			}
		}
		return false;
	}
};

void real_main() {
	sf::Window window( sf::VideoMode( 640, 480 ), "sgsSceneViewer", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
	glewInit();

	glutil::RegisterDebugOutput( glutil::STD_OUT );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 1.0;
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

	KeyAction reloadShadersAction( sf::Keyboard::R, [&] () { sgsSceneRenderer.reloadShaders(); } );
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( reloadShadersAction ) );

	BoolVariableControl showBoundingSpheresToggle( sgsSceneRenderer.debug.showBoundingSpheres, sf::Keyboard::B );
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( showBoundingSpheresToggle ) );

	BoolVariableControl showTerrainBoundingSpheresToggle( sgsSceneRenderer.debug.showTerrainBoundingSpheres, sf::Keyboard::N );
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( showTerrainBoundingSpheresToggle ) );

	BoolVariableControl updateRenderListsToggle( sgsSceneRenderer.debug.updateRenderLists, sf::Keyboard::C );
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( updateRenderListsToggle ) );

	TextureVisualizationWindow optixWindow;
	optixWindow.init( "Optix Version" );
	optixWindow.texture = optixRenderer.debugTexture;

	TextureVisualizationWindow mergedTextureWindow;
	mergedTextureWindow.init( "merged object textures" );
	mergedTextureWindow.texture = sgsSceneRenderer.mergedTexture;

	DebugWindowManager debugWindowManager;
	debugWindowManager.windows.push_back( make_nonallocated_shared( optixWindow ) );
	debugWindowManager.windows.push_back( make_nonallocated_shared( mergedTextureWindow ) );
	
	while (true)
	{
		optixRenderer.renderPinholeCamera( camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() );
		debugWindowManager.update();

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
		
		sgsSceneRenderer.renderShadowmap();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// OpenGL drawing commands go here...
		glMatrixMode( GL_PROJECTION );
		glLoadMatrix( camera.getProjectionMatrix() );

		glMatrixMode( GL_MODELVIEW );
		glLoadMatrix( camera.getViewTransformation().matrix() );
			
		sgsSceneRenderer.render( camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() );

		// End the current frame and display its contents on screen
		window.display();
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