#pragma once

#include <vector>
#include <memory>
#include "make_nonallocated_shared.h"

#include <SFML/Window.hpp>

#include <boost/range/algorithm_ext/erase.hpp>

#include "glObjectWrappers.h"

#include "camera.h"
#include "cameraInputControl.h"
#include "debugRender.h"

#include <unsupported/Eigen/OpenGLSupport>

#include <boost/foreach.hpp>

struct DebugWindowBase {
	sf::Window window;

	typedef float Seconds;
	virtual void update( const Seconds deltaTime, const Seconds elapsedTime ) {
	}
};

struct DisplayListVisualizationWindow : std::enable_shared_from_this<DisplayListVisualizationWindow>, DebugWindowBase {
	GL::DisplayList displayList;
	Camera camera;
	CameraInputControl cameraInputControl;

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

			if( !window.isOpen() ) {
				return;
			}

			cameraInputControl.update( deltaTime, false );
			//update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// OpenGL drawing commands go here...
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( camera.getProjectionMatrix() );
			glMultMatrix( camera.getViewTransformation().matrix() );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			DebugRender::ImmediateCalls::drawCordinateSystem( 1.0 );

			displayList.call();

			// End the current frame and display its contents on screen
			window.display();
		}
	}
};

struct TextureVisualizationWindow : std::enable_shared_from_this<TextureVisualizationWindow>, DebugWindowBase {
	GL::Texture2D texture;

	Camera camera;
	CameraInputControl cameraInputControl;
	
	TextureVisualizationWindow() {}

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

			if( !window.isOpen() ) {
				return;
			}

			cameraInputControl.update( deltaTime, false );
			//update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// OpenGL drawing commands go here...
			if( texture.handle ) {
				glColor4f( 1.0, 1.0, 1.0, 1.0 );

				texture.bind();
				texture.enable();

				glMatrixMode( GL_PROJECTION );
				glLoadIdentity();
				glMatrixMode( GL_MODELVIEW );
				glLoadIdentity();

				DebugRender::ImmediateCalls::drawTexturedScreenQuad();

				texture.unbind();
			}

			// End the current frame and display its contents on screen
			window.display();
		}
	}
};

struct DebugWindowManager {
	std::vector< std::shared_ptr< DebugWindowBase > > windows;

	sf::Clock frameTimer, totalTimer;

	typedef float Seconds;
	void update() {
		// remove expired or closed windows
		boost::remove_erase_if( windows, [] ( const std::shared_ptr< DebugWindowBase > &window) { return !window->window.isOpen(); } );

		BOOST_FOREACH( auto &window, windows ) {
			window->update( frameTimer.restart().asSeconds(), totalTimer.getElapsedTime().asSeconds() );
		}
	}
};