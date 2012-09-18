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

struct TextureVisualizationWindow : std::enable_shared_from_this<TextureVisualizationWindow> {
	Texture2D debugTexture;

	Camera camera;
	CameraInputControl cameraInputControl;
	sf::Window window;

	TextureVisualizationWindow( const Texture2D &debugTexture ) : debugTexture( debugTexture ) {}

	void init( const std::string &caption ) {
		window.create( sf::VideoMode( 640, 480 ), caption.c_str(), sf::Style::Default, sf::ContextSettings(42) );
		window.setActive();

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClearDepth(1.f);

		// init the camera
		camera.perspectiveProjectionParameters.aspect = 640.0f / 480.0f;
		camera.perspectiveProjectionParameters.FoV_y = 75.0f;
		camera.perspectiveProjectionParameters.zNear = 0.1f;
		camera.perspectiveProjectionParameters.zFar = 500.0f;

		// input camera input control
		cameraInputControl.init( make_nonallocated_shared(camera), make_nonallocated_shared(window) );
	}

	typedef float Seconds;
	void update( const Seconds deltaTime, const Seconds elapsedTime ) {
		// process events etc
		if (window.isOpen()) {
			// Activate the window for OpenGL rendering		
			window.setActive();

			// Event processing
			sf::Event event;
			while (window.pollEvent(event))
			{
				// Request for closing the window
				if (event.type == sf::Event::Closed) {
					window.close();
					return;
				}

				if( event.type == sf::Event::Resized ) {
					camera.perspectiveProjectionParameters.aspect = float( event.size.width ) / event.size.height;
					glViewport( 0, 0, event.size.width, event.size.height );
				}

				cameraInputControl.handleEvent( event );
			}

			cameraInputControl.update( deltaTime, false );
			//update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// OpenGL drawing commands go here...
			glColor4f( 1.0, 1.0, 1.0, 1.0 );
			debugTexture.bind();
			debugTexture.enable();

			Program::useFixed();

			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();
			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			DebugRender::ImmediateCalls().drawTexturedUnitQuad();

			debugTexture.unbind();

			// End the current frame and display its contents on screen
			window.display();
		}
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
	cameraInputControl.init( make_nonallocated_shared(camera), make_nonallocated_shared(window) );

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock[2], clock;

	SGSSceneRenderer sgsSceneRenderer;
	SGSScene sgsScene;

	sgsSceneRenderer.reloadShaders();
	{
		boost::timer::auto_cpu_timer loadTimer;

		Serializer::BinaryReader reader( "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene" );

		Serializer::read( reader, sgsScene );

		sgsSceneRenderer.processScene( make_nonallocated_shared( sgsScene ) );
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

	sgsSceneRenderer.optix.debugTexture.load( 0, GL_RGBA8, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	sgsSceneRenderer.optix.debugTexture.parameter( GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	sgsSceneRenderer.optix.debugTexture.parameter( GL_TEXTURE_MIN_FILTER, GL_LINEAR );

	sgsSceneRenderer.initOptix();

	TextureVisualizationWindow optixWindow( sgsSceneRenderer.optix.debugTexture );
	optixWindow.init( "Optix Version" );
	
	while (window.isOpen())
	{
		sgsSceneRenderer.renderOptix( optixWindow.camera.getProjectionMatrix() * optixWindow.camera.getViewTransformation().matrix(), optixWindow.camera.getPosition() );
		optixWindow.update( frameClock[1].restart().asSeconds(), clock.getElapsedTime().asSeconds() );

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

		cameraInputControl.update( frameClock[0].restart().asSeconds(), false );

		// Activate the window for OpenGL rendering
		window.setActive();

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