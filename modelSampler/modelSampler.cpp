#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <iterator>

#include <gtest.h>
#include <Eigen/Eigen>

#include "debugRender.h"

#include <SFML/Window.hpp>

#include "objModel.h"

#include <memory>

#include <boost/assert.hpp>

#include "grid.h"
#include "eigenProjectionMatrices.h"

using namespace Eigen;

#include "camera.h"
#include "cameraInputControl.h"

using namespace niven;

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

class DepthProbeSamples {
	const Grid *grid;

	// xyz
	std::unique_ptr<float[]> depthSamples;
	int numDirections;

public:
	void init( const Grid *grid, int numDirections ) {
		this->grid = grid;
		this->numDirections = numDirections;

		depthSamples.reset( new float[ grid->count * numDirections ] );
	}

	const Grid &getGrid() const {
		return *grid;
	}

	float getSample( int index, int directionIndex ) const {
		return depthSamples[ index * numDirections + directionIndex ];
	}

	float &sample( int index, int directionIndex ) {
		return depthSamples[ index * numDirections + directionIndex ];
	}
};

struct DepthProbleSampler {
	GLuint pbo;

	Grid *grid;
	
	float maxDepth;	

	int numDirections;
	// sorted by main axis xyz, yzx, zxy
	std::vector<Vector3f> directions[3];

	// size: grid.count * numDirections

	typedef GLfloat DepthSample;
	DepthSample* mappedDepthSamples;

	DepthProbeSamples depthSamples;

	DepthSample getMappedDepthSample( int index, int directionIndex ) {
		return mappedDepthSamples[ directionIndex * grid->count + index ];
	}

	void init() {
		numDirections = directions[0].size() + directions[1].size() + directions[2].size();
		depthSamples.init( grid, numDirections );

		glGenBuffers( 1, &pbo );
		glPixelStorei( GL_PACK_ALIGNMENT, 1 );

		size_t volumeSize = sizeof( DepthSample ) * grid->count;
		glNamedBufferDataEXT( pbo, volumeSize * numDirections, nullptr, GL_DYNAMIC_READ );
	}

	void rearrangeMappedDepthSamples() {
		int directionIndex = 0;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };
			
			for( int i = 0 ; i < directions[mainAxis].size() ; ++i, ++directionIndex ) {
				for( int sample = 0 ; sample < grid->count ; ++sample ) {
					const Vector3i index3 = grid->getIndex3( sample );
					const Vector3i targetIndex3 = permute( index3, permutation );
					
					Indexer permutedIndexer = Indexer::fromPermuted( *grid, permutation );
					const int targetIndex = permutedIndexer.getIndex( targetIndex3 );

					depthSamples.sample( sample, directionIndex ) = getMappedDepthSample( targetIndex, directionIndex ) * maxDepth;
				}
			}
		}		 
	}

	static Matrix4f createUniformShearMatrix( const Vector2f &size, const float maxZ, const Vector2f &zStep ) {
		const Vector2f halfSize = size / 2;
		return createShearProjectionMatrix( -halfSize, halfSize, 0.0, maxZ, zStep );
	}

	void sample() {
		int maxTextureSize = 4096;
		BOOST_VERIFY( grid->size.maxCoeff() <= maxTextureSize );

		// create depth renderbuffers and framebuffer objects for each layer
		int numBuffers = grid->size.z() * directions[0].size() + grid->size.x() * directions[1].size() + grid->size.y() * directions[2].size();

		std::unique_ptr<GLuint[]> renderBuffers( new GLuint[numBuffers] );
		std::unique_ptr<GLuint[]> fbos( new GLuint[numBuffers] );
				
		glGenFramebuffers( numBuffers, fbos.get() );
		glGenRenderbuffers( numBuffers, renderBuffers.get() );
		
		glDrawBuffer( GL_NONE );

		OrientedGrid orientedGrid = OrientedGrid::from( *grid );

		int directionIndex = 0;
		DepthSample *offset = nullptr;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			const std::vector<Vector3f> &subDirections = directions[mainAxis];

			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			BOOST_VERIFY( boost::algorithm::all_of( subDirections, [&permutation]( const Vector3f &v ) { return abs( v[permutation[2]] ) > 0.1; } ) );

			OrientedGrid permutedGrid = OrientedGrid::from( orientedGrid, permutation );
			auto invTransformation = permutedGrid.transformation.inverse();

			glBindBuffer( GL_PIXEL_PACK_BUFFER, pbo );
		
			glPushAttrib(GL_VIEWPORT_BIT);
			glViewport(0, 0, permutedGrid.size[0], permutedGrid.size[1]); 
		
			for( int subDirectionIndex = 0 ; subDirectionIndex < directions[mainAxis].size() ; ++subDirectionIndex, ++directionIndex ) {
				const Vector3f direction = subDirections[subDirectionIndex];
				const Vector3f permutedDirection = invTransformation.linear() * direction.normalized() * maxDepth;
			
				// set the projection matrix
				glMatrixMode( GL_PROJECTION );
				const float shearedMaxDepth = permutedDirection[2];
				glLoadMatrix( createShearProjectionMatrix( Vector2f::Zero(), permutedGrid.size.head<2>().cast<float>(), 0, shearedMaxDepth, permutedDirection.head<2>() / shearedMaxDepth ) );
			
				glMatrixMode( GL_MODELVIEW );

				for( int i = 0 ; i < permutedGrid.size[2] ; ++i ) {
					glBindFramebuffer( GL_FRAMEBUFFER, fbos[i] );
					glBindRenderbuffer( GL_RENDERBUFFER, renderBuffers[i] );
					glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, permutedGrid.size[0], permutedGrid.size[1] );
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffers[i] );

					glClear( GL_DEPTH_BUFFER_BIT );	

					glLoadIdentity();
					// looking down the negative z axis by default --- so flip if necessary (this changes winding though!!!)
					glScalef( 1.0, 1.0, permutedDirection.z() > 0 ? -1.0 : 1.0 );

					// pixel alignment and "layer selection"
					glTranslatef( 0.5, 0.5, -i );
					
					glMultMatrix( invTransformation );

					debugScene.render();

					glReadPixels( 0, 0, permutedGrid.size[0], permutedGrid.size[1], GL_DEPTH_COMPONENT, GL_FLOAT, (GLvoid*) offset );

					offset += permutedGrid.size.head<2>().prod();
				}
			}
			glPopAttrib();
		}

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );


		glDrawBuffer( GL_BACK );

		glDeleteFramebuffers( numBuffers, fbos.get() );
		glDeleteRenderbuffers( numBuffers, renderBuffers.get() );

		mappedDepthSamples = (DepthSample*) glMapNamedBufferEXT( pbo, GL_READ_ONLY );
		rearrangeMappedDepthSamples();
		glUnmapNamedBufferEXT( pbo );
	}
};

#include "niven.Core.Core.h"

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
	Core::Initialize ();

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
	//debugScene.drawBox( Vector3f::Constant(8), false, true );
	debugScene.drawSolidSphere( 8.0 );
	debugScene.end();

	Grid grid( Vector3i( 4, 8, 16 ), Vector3f( 0.0, 0.0, 0.0 ), 0.25 );

	DepthProbleSampler sampler;
	sampler.grid = &grid;
	sampler.directions[0].push_back( Vector3f( 0.3, 0.0, 1.0 ) );
	sampler.directions[0].push_back( Vector3f( -0.3, 0.0, 1.0 ) );
	sampler.directions[1].push_back( Vector3f( 1.0, 0.0, -0.3 ) );
	sampler.directions[1].push_back( Vector3f( 1.0, 0.0, 0.3 ) );
	sampler.directions[2].push_back( Vector3f( 0.0, 1.0, -0.3 ) );
	sampler.directions[2].push_back( Vector3f( 0.0, 1.0, 0.3 ) );

	sampler.maxDepth = 20;

	sampler.init();
	sampler.sample();

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
						depthInfo.drawVector( sampler.depthSamples.getSample( grid.getIndex( index3 ), directionIndex ) * direction );
					}
				}
			}
		}
		depthInfo.end();

		// End the current frame and display its contents on screen
		window.display();
	}

};