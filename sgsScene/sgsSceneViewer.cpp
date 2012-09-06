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

struct null_deleter {
	template<typename T>
	void operator() (T*) {}
};

template<typename T>
std::shared_ptr<T> shared_from_stack(T &object) {
	return std::shared_ptr<T>( &object, null_deleter() );
}

#include "sgsSceneRender.h"

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

void main() {
	sf::Window window( sf::VideoMode( 640, 480 ), "sgsSceneViewer", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
	glewInit();

	glutil::RegisterDebugOutput( glutil::STD_OUT );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 1.0;
	camera.perspectiveProjectionParameters.zFar = 500.0;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( shared_from_stack(camera), shared_from_stack(window) );

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	SGSSceneRenderer sgsSceneRenderer;
	SGSScene sgsScene;

	{
		boost::timer::auto_cpu_timer loadTimer;

		Serializer::BinaryReader reader( "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene" );

		Serializer::read( reader, sgsScene );

		sgsSceneRenderer.processScene( sgsScene );
	}

	EventDispatcher eventDispatcher;
	eventDispatcher.eventHandlers.push_back( shared_from_stack( cameraInputControl ) );
	while (window.isOpen())
	{
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

		cameraInputControl.update( frameClock.restart().asSeconds(), false );

		// Activate the window for OpenGL rendering
		window.setActive();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// OpenGL drawing commands go here...
		glMatrixMode( GL_PROJECTION );
		glLoadMatrix( camera.getProjectionMatrix() );

		glMatrixMode( GL_MODELVIEW );
		glLoadMatrix( camera.getViewTransformation().matrix() );

		sgsSceneRenderer.render();

		// End the current frame and display its contents on screen
		window.display();
	}
};