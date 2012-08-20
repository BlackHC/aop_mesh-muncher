#include <iostream>

#include <functional>
#include <iterator>

#include <algorithm>
#include <Eigen/Eigen>
#include <memory>

#include <gtest.h>

#include "debugRender.h"

#include <SFML/Window.hpp>

#include "positionSolver.h"

#include "camera.h"
#include "eventHandling.h"

using namespace Eigen;

struct null_deleter {
	template<typename T>
	void operator() (T*) {}
};

template<typename T>
std::shared_ptr<T> shared_from_stack(T &object) {
	return std::shared_ptr<T>( &object, null_deleter() );
}

struct MouseCapture : public EventHandler  {
	typedef MouseCapture super;

	std::shared_ptr<sf::Window> window;

	MouseCapture() : captureMouse( false ) {}

	void init( const std::shared_ptr<sf::Window> &window ) {
		this->window = window;
	}
	
	bool getCapture() const {
		return captureMouse;
	}

	sf::Vector2i getMouseDelta() {
		sf::Vector2i temp = mouseDelta;
		mouseDelta = sf::Vector2i();
		return temp;
	}

	void setCapture( bool active ) {
		if( captureMouse == active ) {
			return;
		}

		mouseDelta = sf::Vector2i();

		if( active ) {
			oldMousePosition = sf::Mouse::getPosition();
		}
		// TODO: add support for ClipCursor?
		window->setMouseCursorVisible( !active );
		
		captureMouse = active;
	}

	bool handleEvent( const sf::Event &event ) {
		switch( event.type ) {
		case sf::Event::MouseLeft:
			if( captureMouse ) {
				sf::Mouse::setPosition( sf::Vector2i( window->getSize() / 2u ), *window );	
				oldMousePosition = sf::Mouse::getPosition();
			}
			return true;
		case sf::Event::MouseMoved:
			if( captureMouse ) {
				mouseDelta += sf::Mouse::getPosition() - oldMousePosition;
				oldMousePosition = sf::Mouse::getPosition();
			}			
			return true;
		}
		return false;
	}

private:
	sf::Vector2i mouseDelta, oldMousePosition;
	bool captureMouse;
};

struct CameraInputControl : public MouseCapture {
	std::shared_ptr<Camera> camera;

	void init( const std::shared_ptr<Camera> &camera, const std::shared_ptr<sf::Window> &window ) {
		super::init( window );
		this->camera = camera;
	}

	bool handleEvent( const sf::Event &event ) {
		if( super::handleEvent( event ) ) {
			return true;
		}

		switch( event.type ) {
		case sf::Event::LostFocus:
			setCapture( true );
			break;
		case sf::Event::KeyPressed:
			if( event.key.code == sf::Keyboard::Escape ) {
				setCapture( false );
			}
			return true;
		case sf::Event::MouseButtonReleased:
			if( event.mouseButton.button == sf::Mouse::Left ) {
				setCapture( true );
			}
			return true;
		}
		return false;
	}
	
	bool update( const float elapsedTime, bool inputProcessed ) {
		if( !inputProcessed && getCapture() ) {
			Eigen::Vector3f relativeMovement = Vector3f::Zero();
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::W ) ) {
				relativeMovement.z() -= 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::S ) ) {
				relativeMovement.z() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::A ) ) {
				relativeMovement.x() -= 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::D ) ) {
				relativeMovement.x() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::Space ) ) {
				relativeMovement.y() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::LControl ) ) {
				relativeMovement.y() -= 1;
			}
					
			relativeMovement *= elapsedTime * 10;

			Eigen::Vector3f newPosition = camera->getPosition() + camera->getViewTransformation().linear().transpose() * relativeMovement;
			camera->setPosition( newPosition );

			sf::Vector2f angleDelta = sf::Vector2f( getMouseDelta() ) * 0.5f;

			camera->yaw( angleDelta.x );
			camera->pitch( angleDelta.y );
		}

		return true;
	}
};

void main() {
	std::vector<Point> points;

	points.push_back( Point( Vector3f(0,0,0), 5 ) );
	points.push_back( Point( Vector3f(0,0,0), 5 ) );
	points.push_back( Point( Vector3f(0,0,0), 5 ) );
	points.push_back( Point( Vector3f(0,0,0), 5 ) );

	points.push_back( Point( Vector3f(10,0,0), 5 ) );
	points.push_back( Point( Vector3f(5,0,5), 5 ) );

	points.push_back( Point( Vector3f(0,6,0), 5 ) );
	points.push_back( Point( Vector3f(10,6,0), 5 ) );
	points.push_back( Point( Vector3f(5,6,5), 5 ) );

	points.push_back( Point( Vector3f(0,6.5,0), 5 ) );
	points.push_back( Point( Vector3f(10,6.5,0), 5 ) );
	points.push_back( Point( Vector3f(5,6.5,5), 5 ) );

	points.push_back( Point( Vector3f(0,0,0), 5 ) );
	points.push_back( Point( Vector3f(11.8,0,0), 5 ) );

	const float halfThickness = 1;
	auto results = solveIntersectionsWithPriority( points, halfThickness, 0.1 );

	sf::Window window( sf::VideoMode( 640, 480 ), "Position Solver", sf::Style::Default, sf::ContextSettings(32) );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 0.0001f;
	camera.perspectiveProjectionParameters.zFar = 500.0;
	//camera.position.z() = 5;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( shared_from_stack(camera), shared_from_stack(window) );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	DebugRender::CombinedCalls debugSourcePoints;
	debugSourcePoints.begin();
	// add source points
	for( auto point = points.begin() ; point != points.end() ; ++point ) {
		debugSourcePoints.setPosition( point->center );

		debugSourcePoints.setColor( Eigen::Vector3f( 0.0, 0.0, 1.0 ) );
		debugSourcePoints.drawWireframeSphere( point->distance + halfThickness );

		debugSourcePoints.setColor( Eigen::Vector3f( 0.0, 0.0, 0.5 ) );
		debugSourcePoints.drawWireframeSphere( point->distance - halfThickness );
	}
	debugSourcePoints.end();

	DebugRender::CombinedCalls debugResults;
	debugResults.begin();
	for( auto sparseCellInfo = results.begin() ; sparseCellInfo != results.end() ; ++sparseCellInfo ) {
		debugResults.setColor( Eigen::Vector3f( 1.0, 0.0, 0.0 ) * ( float(sparseCellInfo->upperBound + sparseCellInfo->lowerBound) / 2 / points.size() * 0.75 + 0.25 ) );
		debugResults.drawAABB( sparseCellInfo->minCorner, sparseCellInfo->minCorner + Vector3f::Constant( sparseCellInfo->resolution ) );
	}
	debugResults.end();

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;
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

			cameraInputControl.handleEvent( event );
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

		debugSourcePoints.render();
		debugResults.render();

		// End the current frame and display its contents on screen
		window.display();
	}

};