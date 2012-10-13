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

#include "verboseEventHandlers.h"

#include <unsupported/Eigen/OpenGLSupport>

#include <boost/foreach.hpp>
#include "boost/algorithm/string/join.hpp"

struct DebugWindowBase {
	typedef std::shared_ptr< DebugWindowBase > SPtr;

	sf::Window window;

	typedef float Seconds;
	virtual void update( const Seconds deltaTime, const Seconds elapsedTime ) {
	}
};

struct DisplayListVisualizationWindow : std::enable_shared_from_this<DisplayListVisualizationWindow>, DebugWindowBase {
	GL::DisplayList displayList;
	Camera camera;
	CameraInputControl cameraInputControl;
	EventSystem eventSystem;

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
		cameraInputControl.init( make_nonallocated_shared(camera) );

		eventSystem.setRootHandler( make_nonallocated_shared( cameraInputControl ) );
		eventSystem.exclusiveMode.window = make_nonallocated_shared( window );
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

				eventSystem.processEvent( event );
			}

			if( !window.isOpen() ) {
				return;
			}

			eventSystem.update( deltaTime, elapsedTime );
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

struct MultiDisplayListVisualizationWindow : std::enable_shared_from_this<MultiDisplayListVisualizationWindow>, DebugWindowBase {
	struct Visualization {
		GL::DisplayList displayList;
		std::string name;

		Visualization() {}
	};
	Visualization visualizations[10];

	struct Logic {
		int disableMask;
		int enabledMask;
		int toggleMask;
	};
	Logic keyLogics[10];

	int mask;

	void makeVisible( int i ) {
		mask |= 1<<i;
	}

	Camera camera;
	CameraInputControl cameraInputControl;
	EventSystem eventSystem;
	EventDispatcher eventDispatcher;

	MultiDisplayListVisualizationWindow() : mask() {
		for( int i = 0 ; i < 10 ; i++ ) {
			keyLogics[i].toggleMask = 1<<i;
		}
	}

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
		cameraInputControl.init( make_nonallocated_shared(camera) );

		eventSystem.exclusiveMode.window = make_nonallocated_shared( window );
		eventSystem.setRootHandler( make_nonallocated_shared( eventDispatcher ) );
		eventDispatcher.addEventHandler( make_nonallocated_shared( cameraInputControl ) );

		registerConsoleHelpAction( eventDispatcher );

		for( int i = 0 ; i < 10 ; i++ ) {
			Logic &keyLogic = keyLogics[ i ];

			keyLogic.disableMask &= ~(keyLogic.enabledMask | keyLogic.toggleMask);
			keyLogic.toggleMask &= ~keyLogic.enabledMask;
		}

		for( int i = 0 ; i < 10 ; i++ ) {
			const Logic &keyLogic = keyLogics[ i ];

			std::vector< std::string > effects;
			for( int visIndex = 0 ; visIndex < 10 ; visIndex++ ) {
				int bit = 1 << visIndex;
				if( keyLogic.toggleMask & bit ) {
					effects.push_back( "toggle " + visualizations[ visIndex ].name );
				}
				else if( keyLogic.enabledMask & bit ) {
					effects.push_back( "enable " + visualizations[ visIndex ].name );
				}
				else if( keyLogic.disableMask & bit ) {
					effects.push_back( "disable " + visualizations[ visIndex ].name );
				}
			}
			
			auto action = std::make_shared<KeyAction>(
				boost::join( effects, ", " ),
				sf::Keyboard::Key( sf::Keyboard::Num0 + i ),
				[&, i] () {
					const Logic &logic = keyLogics[ i ];
					this->mask = ((mask & ~logic.disableMask) ^ logic.toggleMask) | logic.enabledMask;
				}
			);
			eventDispatcher.addEventHandler( action );
		}
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

				eventSystem.processEvent( event );
			}

			if( !window.isOpen() ) {
				return;
			}

			eventSystem.update( deltaTime, elapsedTime );
			//update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// OpenGL drawing commands go here...
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( camera.getProjectionMatrix() );
			glMultMatrix( camera.getViewTransformation().matrix() );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			DebugRender::ImmediateCalls::drawCordinateSystem( 1.0 );

			for( int i = 0 ; i < 10 ; i++ ) {
				const Visualization &visualization = visualizations[ i ];
				if( mask & (1<<i) ) {
					visualization.displayList.call();
				}
			}

			// End the current frame and display its contents on screen
			window.display();
		}
	}

	~MultiDisplayListVisualizationWindow() {
		for( int i = 0 ; i < 10 ; i++ ) {
			visualizations[i].displayList.release();
		}
	}
};

struct TextureVisualizationWindow : std::enable_shared_from_this<TextureVisualizationWindow>, DebugWindowBase {
	GL::Texture2D texture;

	TextureVisualizationWindow() {}

	void init( const std::string &caption ) {
		window.create( sf::VideoMode( 640, 480 ), caption.c_str(), sf::Style::Default, sf::ContextSettings(42) );
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
					glViewport( 0, 0, event.size.width, event.size.height );
				}
			}

			if( !window.isOpen() ) {
				return;
			}

			glClear(GL_COLOR_BUFFER_BIT );

			// OpenGL drawing commands go here...
			if( texture.handle ) {
				glColor4f( 1.0, 1.0, 1.0, 1.0 );

				texture.bind();
				texture.enable();

				glMatrixMode( GL_PROJECTION );
				glLoadIdentity();
				glMatrixMode( GL_MODELVIEW );
				glLoadIdentity();

				DebugRender::drawTexturedScreenQuad();

				texture.unbind();
			}

			// End the current frame and display its contents on screen
			window.display();
		}
	}
};

struct DebugWindowManager {
	void add( DebugWindowBase::SPtr window ) {
		windows.push_back( window );
	}

	void remove( DebugWindowBase *window ) {
		for( auto myWindow = windows.begin() ; myWindow != windows.end() ; ++myWindow ) {
			if( myWindow->get() == window ) {
				windows.erase( myWindow );
				return;
			}
		}
	}

	std::vector< DebugWindowBase::SPtr > windows;

	sf::Clock frameTimer, totalTimer;

	typedef float Seconds;
	void update() {
		// remove expired or closed windows
		boost::remove_erase_if( windows, [] ( const std::shared_ptr< DebugWindowBase > &window) { return !window->window.isOpen(); } );

		Seconds frameTime = frameTimer.restart().asSeconds();
		Seconds totalTime = totalTimer.getElapsedTime().asSeconds();

		BOOST_FOREACH( auto &window, windows ) {
			window->update( frameTime, totalTime );
		}
	}
};