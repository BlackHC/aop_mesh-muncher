#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <iterator>

#include <Eigen/Eigen>

#include "debugRender.h"

#include <SFML/Window.hpp>

#include <memory>

#include "colorAndDepthSampler.h"

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

DebugRender::CombinedCalls debugScene;
// TODO: add unit tests for the shear matrix function


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
	sf::Window window( sf::VideoMode( 640, 480 ), "Position Solver", sf::Style::Default, sf::ContextSettings(32) );
	glewInit();

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 1.0;
	camera.perspectiveProjectionParameters.zFar = 500.0;
	//camera.position.z() = 5;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( shared_from_stack(camera), shared_from_stack(window) );

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	debugScene.begin();
	glRotatef( 45.0, 0.0, 1.0, 0.0 );
	const double scaleFactor = sqrt(2.0);
	debugScene.drawBox( Vector3f( 8 * scaleFactor, 8, 8 * scaleFactor ), false, true );
	debugScene.end();

	//Grid grid( Vector3i( 4, 8, 16 ), Vector3f( 0.0, 0.0, 0.0 ), 0.25 );
	OrientedGrid grid = OrientedGrid::from( Vector3i(7, 7, 7), Vector3f(-3, -3, -3), 1.0 );

	ColorAndDepthSampler sampler;
	sampler.grid = &grid;
	/*sampler.directions[0].push_back( Vector3f( 0.3, 0.0, -1.0 ) )
	sampler.directions[0].push_back( Vector3f( -0.3, 0.0, -1.0 ) );
	/*sampler.directions[1].push_back( Vector3f( 1.0, 0.0, -0.3 ) );
	sampler.directions[1].push_back( Vector3f( 1.0, 0.0, 0.3 ) );
	sampler.directions[2].push_back( Vector3f( 0.0, 1.0, -0.3 ) );
	sampler.directions[2].push_back( Vector3f( 0.0, 1.0, 0.3 ) );*/
	sampler.directions[0].push_back( Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( -Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );

	sampler.maxDepth = 20;

	sampler.init();
	sampler.sample( [&]() { debugScene.render(); } );

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	int z = 0;
	IntVariableControl zControl( &z, 0, grid.size[2] - 1, sf::Keyboard::Numpad7, sf::Keyboard::Numpad1 );

	EventDispatcher eventDispatcher;
	eventDispatcher.eventHandlers.push_back( shared_from_stack( cameraInputControl ) );
	eventDispatcher.eventHandlers.push_back( shared_from_stack( zControl ) );
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

		debugScene.render();
		
		DebugRender::ImmediateCalls depthInfo;
		depthInfo.begin();
		depthInfo.setColor( Vector3f( 1.0, 1.0, 1.0 ) );
		int directionIndex = 0;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			for( int subDirectionIndex = 0 ; subDirectionIndex < sampler.directions[mainAxis].size() ; ++subDirectionIndex, ++directionIndex ) {
				const Vector3f &direction = sampler.directions[mainAxis][subDirectionIndex].normalized();

				for( int x = 0 ; x < grid.size[0] ; x++ ) {
					for( int y = 0 ; y < grid.size[1] ; y++ ) {
						const Vector3i index3 = Vector3i( x, y, z );
						depthInfo.setPosition( grid.getPosition( index3 ) );

						auto &sample = sampler.colorAndDepthSamples.getSample( grid.getIndex( index3 ), directionIndex );
						glColor3ubv( &sample.color.r );
						depthInfo.drawVector( sample.depth * (grid.indexToPosition.linear() * direction) );
					}
				}
			}
		}
		depthInfo.end();

		// End the current frame and display its contents on screen
		window.display();
	}

};