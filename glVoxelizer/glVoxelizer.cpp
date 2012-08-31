#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <iterator>

#include <boost/foreach.hpp>
#define boost_foreach BOOST_FOREACH

#include <boost/range.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>

#include <boost/format.hpp>

#include <Eigen/Eigen>

#include "debugRender.h"

#include <SFML/Window.hpp>

#include <memory>

using namespace Eigen;

#include "camera.h"
#include "cameraInputControl.h"

#include "voxelizer.h"

#include "Debug.h"

typedef GridStorage<Vector3f> ColorGridStorage;
typedef GridStorage<unsigned __int32> HitCountGrid;

// wrap_nonallocated_shared?
template<typename T>
std::shared_ptr<T> make_nonallocated_shared(T &object) {
	// make this private for now
	struct null_deleter {		
		void operator() (T*) {}
	};

	return std::shared_ptr<T>( &object, null_deleter() );
}

DebugRender::CombinedCalls debugScene;
DebugRender::CombinedCalls hitVisualization;

struct IntVariableControl : EventHandler {
	sf::Keyboard::Key upKey, downKey;
	int &variable;
	int min, max;

	IntVariableControl( int &variable, int min, int max, sf::Keyboard::Key upKey = sf::Keyboard::Up, sf::Keyboard::Key downKey = sf::Keyboard::Down )
		: variable( variable ), min( min ), max( max ), upKey( upKey ), downKey( downKey ) {
	}

	virtual bool handleEvent( const sf::Event &event ) 
	{
		switch( event.type ) {
		case sf::Event::KeyPressed:
			if( event.key.code == upKey ) {
				variable = std::min( variable + 1, max );
				return true;
			}
			else if( event.key.code == downKey ) {
				variable = std::max( variable - 1, min );
				return false;
			}
			break;
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

void visualizeColorGrid( const ColorGrid &grid, DebugRender::CombinedCalls &dr ) {
	const float size = grid.getMapping().getResolution();
	dr.begin();
	for( auto iterator = grid.getIterator() ; iterator.hasMore() ; ++iterator ) {
		const auto &color = grid[ *iterator ];

		if( color.a != 0 ) {
			dr.setPosition( grid.getMapping().getPosition( iterator.getIndex3() ) );		
			glColor3ub( color.r, color.g, color.b );
			//dr.drawAbstractSphere( size );
			dr.drawBox( Vector3f::Constant( size ), false );
		}
	}
	dr.end();
}
void visualizeHitCountGrid( const HitCountGrid &hitCountGrid, DebugRender::CombinedCalls &dr ) {
	const float size = hitCountGrid.getMapping().getResolution();
	dr.begin();
	for( auto iterator = hitCountGrid.getIterator() ; iterator.hasMore() ; ++iterator ) {
		const unsigned int &hitCount = hitCountGrid[ *iterator ];
		
		if( hitCount ) {
			dr.setPosition( hitCountGrid.getMapping().getPosition( iterator.getIndex3() ) );		
			dr.setColor( Vector3f(1.0, 0.0, 0.0) * (0.2f + (float) hitCount / 6.0f ) + Vector3f(0.0, 1.0, 0.0) * (float) hitCount / 25.0f + Vector3f(0.0, 0.0, 1.0) * (float) hitCount / 125.0f );
			//dr.drawAbstractSphere( size );
			dr.drawBox( Vector3f::Constant( size ), false );
		}
	}
	dr.end();
}

struct SimpleVisualizationWindow : std::enable_shared_from_this<SimpleVisualizationWindow> {
	DebugRender::CombinedCalls data;
	Camera camera;
	CameraInputControl cameraInputControl;
	sf::Window window;

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
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( camera.getProjectionMatrix() );
			glMultMatrix( camera.getViewTransformation().matrix() );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			DebugRender::ImmediateCalls ic;
			ic.drawCordinateSystem( 1.0 );

			data.render();

			// End the current frame and display its contents on screen
			window.display();
		}
	}
};

struct SimpleVisualizationWindowManager {
	std::vector< std::shared_ptr< SimpleVisualizationWindow > > windows;

	sf::Clock frameTime, totalTime;
	void update() {
		// remove expired or closed windows
		boost::remove_erase_if( windows, [] ( const std::shared_ptr< SimpleVisualizationWindow > &window) { return !window->window.isOpen(); } );

		boost_foreach( auto &window, windows ) {
			window->update( frameTime.restart().asSeconds(), totalTime.getElapsedTime().asSeconds() );
		}
	}
};

void main() {
	sf::Window window( sf::VideoMode( 640, 480 ), "glVoxelizer", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
	glewInit();

	glutil::RegisterDebugOutput( glutil::STD_OUT );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0f / 480.0f;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 1.0;
	camera.perspectiveProjectionParameters.zFar = 500.0;
	//camera.position.z() = 5;

	bool drawScene = true, drawVoxels = true;
	BoolVariableControl sceneToggle( drawScene, sf::Keyboard::C ), voxelToggle( drawVoxels, sf::Keyboard::V );

	CameraInputControl cameraInputControl;
	cameraInputControl.init( make_nonallocated_shared(camera), make_nonallocated_shared(window) );

	SimpleVisualizationWindowManager visManager;

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	debugScene.begin();
	//debugScene.drawBox( Vector3f( 8, 8, 8 ), false, true );
	debugScene.drawSolidSphere( 8.0 );
	debugScene.end();

	SimpleIndexMapping3 indexMapping = createIndexMapping( Vector3i::Constant( 8 * 16 + 1 ), Vector3f::Constant( -8.0f ), 0.125f );

	ColorGrid colorGrid;
	voxelizeScene( indexMapping, colorGrid, std::bind( &DebugRender::CombinedCalls::render, &debugScene ) );
	visualizeColorGrid( colorGrid, hitVisualization );

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	EventDispatcher eventDispatcher;
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( cameraInputControl ) );
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( sceneToggle ) );
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( voxelToggle ) );
	while (window.isOpen())
	{
		visManager.update();

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

			if( event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::R ) {
				voxelizeScene( indexMapping, colorGrid, std::bind( &DebugRender::CombinedCalls::render, debugScene ) );
				visualizeColorGrid( colorGrid, hitVisualization );
			}

			eventDispatcher.handleEvent( event );
		}

		cameraInputControl.update( frameClock.restart().asSeconds(), false );

		// Activate the window for OpenGL rendering
		window.setActive();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//voxelizeScene( indexMapping );

		// OpenGL drawing commands go here...
		glMatrixMode( GL_PROJECTION );
		glLoadMatrix( camera.getProjectionMatrix() );

		glMatrixMode( GL_MODELVIEW );
		glLoadMatrix( camera.getViewTransformation().matrix() );		

		if( drawScene )
			debugScene.render();

		if( drawVoxels )
			hitVisualization.render();
		
		// End the current frame and display its contents on screen
		window.display();
	}
};