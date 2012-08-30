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

#include "gridStorage.h"

#include "glHelpers.h"

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

void visualizeHitCountGrid( const HitCountGrid &hitCountGrid, DebugRender::CombinedCalls &dr ) {
	const float size = 0.25f * hitCountGrid.getMapping().getResolution();
	dr.begin();
	for( auto iterator = hitCountGrid.getIterator() ; iterator.hasMore() ; ++iterator ) {
		const unsigned int &hitCount = hitCountGrid[ *iterator ];
		if( !hitCount ) {
			continue;
		}
		dr.setPosition( hitCountGrid.getMapping().getPosition( iterator.getIndex3() ) );
		dr.setColor( Vector3f(1.0, 0.0, 0.0) * (0.2f + hitCount / 6.0f ) + Vector3f(0.0, 1.0, 0.0) * hitCount / 25.0f + Vector3f(0.0, 0.0, 1.0) * hitCount / 125.0f );
		dr.drawAbstractSphere( size );
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

struct SplatShader : Shader {
	void init() {
		Shader::init( "voxelizer.glsl", "#define SPLAT_PROGRAM\n", true );
	}

	GLuint mainAxisProjection[3];
	GLuint volumeChannels[4];

	void setLocations() {
		for( int i = 0 ; i < 3 ; ++i ) {
			mainAxisProjection[i] = glGetUniformLocation( program, boost::str( boost::format( "mainAxisProjection[%i]" ) % i ).c_str() );
		}
		for( int i = 0 ; i < 4 ; ++i ) {
			volumeChannels[i] = glGetUniformLocation( program, boost::str( boost::format( "volumeChannels[%i]" ) % i ).c_str() );
		}
	}
};

void voxelizeScene( const SimpleOrientedIndexMapping3 &indexMapping3 ) {
	glPushAttrib( GL_ALL_ATTRIB_BITS );

	glPixelStorei( GL_PACK_ALIGNMENT, 1 );

	SplatShader splatShader;
	splatShader.init();

	unsigned __int32 *volumeBuffer[4];

	splatShader.apply();

	GLuint volumeChannels[4];
	glGenTextures( 4, volumeChannels );
	for( int i = 0 ; i < 4 ; ++i ) {
		volumeBuffer[i] = new unsigned __int32[ indexMapping3.count ];
		memset( volumeBuffer[i], 0, sizeof( __int32 ) * indexMapping3.count );

		glBindTexture( GL_TEXTURE_3D, volumeChannels[i] );
		glTexImage3D( GL_TEXTURE_3D, 0, GL_R32UI, indexMapping3.getSize().x(), indexMapping3.getSize().y(), indexMapping3.getSize().z(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, volumeBuffer[i] );

		glBindImageTexture( i, volumeChannels[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI );
		
		glUniform1i( splatShader.volumeChannels[i], i );
	}

	glBindTexture( GL_TEXTURE_3D, 0 );

	GLuint fbo;
	glGenFramebuffers( 1, &fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	
	GLuint renderbuffer;
	
	glGenRenderbuffers( 1, &renderbuffer );
	glBindRenderbuffer( GL_RENDERBUFFER, renderbuffer );
	int maxSize = indexMapping3.getSize().maxCoeff();
	glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA, maxSize, maxSize );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer );
	// disable framebuffer operations
	glDisable( GL_DEPTH_TEST );
	glDrawBuffer( GL_NONE );
	glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	glDepthMask( GL_FALSE );

	// GL_PROJECTION is not needed
	int permutations[3][3] = { {2,1,0}, {2,0,1}, {0,1,2} };
	for( int i = 0 ; i < 3 ; ++i ) {
		int *permutation = permutations[i];
		const Vector3i permutedSize = permute( indexMapping3.getSize(), permutation );
		auto projection = Eigen::createOrthoProjectionMatrixLH( Vector2f::Constant( -0.5 ), permutedSize.head<2>().cast<float>() + Vector2f::Constant( -0.5 ), 0, permutedSize.z() ) * unpermutedToPermutedMatrix( permutation ); 
		glUniform( splatShader.mainAxisProjection[i], projection );

		glViewportIndexedf( i, 0, 0, permutedSize.x(), permutedSize.y() );
	}

	glMatrixMode( GL_MODELVIEW );
	glLoadMatrix( indexMapping3.positionToIndex );
	
	debugScene.render();
	
	glUseProgram( 0 );

	glBindTexture( GL_TEXTURE_3D, volumeChannels[3] );
	glGetTexImage( GL_TEXTURE_3D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, volumeBuffer[3] );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glPopAttrib();

	HitCountGrid hitGrid( indexMapping3, volumeBuffer[3] );
	visualizeHitCountGrid( hitGrid, hitVisualization );

	delete[] volumeBuffer[0];
	delete[] volumeBuffer[1];
	delete[] volumeBuffer[2];

	glDeleteTextures( 4, volumeChannels );

	glDeleteFramebuffers( 1, &fbo );
	glDeleteRenderbuffers( 1, &renderbuffer );
}

void main() {
	sf::Window window( sf::VideoMode( 640, 480 ), "glVoxelizer", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
	glewInit();

	glutil::RegisterDebugOutput( glutil::STD_OUT );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 1.0;
	camera.perspectiveProjectionParameters.zFar = 500.0;
	//camera.position.z() = 5;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( make_nonallocated_shared(camera), make_nonallocated_shared(window) );

	SimpleVisualizationWindowManager visManager;

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	debugScene.begin();
	debugScene.drawBox( Vector3f( 8, 8, 8 ), false, true );
	//debugScene.drawSolidSphere( 8.0 );
	debugScene.end();

	SimpleOrientedIndexMapping3 indexMapping = createOrientedIndexMapping( Vector3i::Constant( 32 ), Vector3f::Constant( -8.0f ), 0.5f );
	voxelizeScene( indexMapping );

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	EventDispatcher eventDispatcher;
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( cameraInputControl ) );
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

		hitVisualization.render();
		
		// End the current frame and display its contents on screen
		window.display();
	}
};