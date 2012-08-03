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

using namespace Eigen;

struct Grid {
	Vector3i size;
	int count;

	Vector3f offset;
	float resolution;

	Grid( const Vector3i &size, const Vector3f &offset, float resolution ) : size( size ), offset( offset ), resolution( resolution ) {
		count = size[0] * size[1] * size[2];
	}

	int getIndex( const Vector3i &index3 ) {
		return index3[0] + size[0] * index3[1] + (size[0] * size[1]) * index3[2];
	}

	Vector3i getIndex3( int index ) {
		int x = index % size[0];
		index /= size[0];
		int y = index % size[1];
		index /= size[1];
		int z = index;
		return Vector3i( x,y,z );
	}

	Vector3f getPosition( const Vector3i &index3 ) {
		return offset + index3.cast<float>() * resolution;
	}
};

const Vector3i indexToCubeCorner[] = {
	Vector3i( 0,0,0 ), Vector3i( 1,0,0 ), Vector3i( 0,1,0 ), Vector3i( 0,0,1 ),
	Vector3i( 1,1,0 ), Vector3i( 0,1,1 ), Vector3i( 1,0,1 ), Vector3i( 1,1,1 )
};

inline float squaredMinDistanceAABoxPoint( const Vector3f &min, const Vector3f &max, const Vector3f &point ) {
	Vector3f distance;
	for( int i = 0 ; i < 3 ; i++ ) {
		if( point[i] > max[i] ) {
			distance[i] = point[i] - max[i];
		}
		else if( point[i] > min[i] ) {
			distance[i] = 0.f;
		}
		else {
			distance[i] = min[i] - point[i];
		}
	}
	return distance.squaredNorm();
}

inline float squaredMaxDistanceAABoxPoint( const Vector3f &min, const Vector3f &max, const Vector3f &point ) {
	Vector3f distanceA = (point - min).cwiseAbs();
	Vector3f distanceB = (point - max).cwiseAbs();
	Vector3f distance = distanceA.cwiseMax( distanceB );

	return distance.squaredNorm();
}

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

/*
struct DepthProbleSampler {
	GLuint fbo;

	Grid *grid;
	
	int numDirections;

	GLuint depthTextures[3];

	std::vector<Vector3f> directions;
	// sorted by main axis
	std::vector<Vector3f> separatedDirections[3];
	// used to convert the depth buffer values into a real depth value (according to near/far plane and the shear matrix)
	std::vector<float> depthScale;

	// size: numDirections * grid.count
	std::vector<float> orderedSamples;

	void sample( const std::vector<Vector3f> &directions ) {
		numDirections = directions.size();
		this->directions = directions;
		
		// fill separatedDirections
		

		int maxTextureSize = 4096;
		BOOST_ASSERT( grid->size.maxCoeff() <= maxTextureSize );

		// create depth textures for each axis
		Vector3i sizes[3];
		for( int axis = 0 ; axis < 3 ; ++axis ) {
			sizes[axis] = Vector3i( grid->size[ (axis + 1) % 3 ], grid->size[ (axis + 2) % 3 ], grid->size[ axis ] );
			glTexStorage3D( GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT32F, sizes[axis][0], sizes[axis][1], sizes[axis][2] );
		}

		
	}
};*/

DebugRender::CombinedCalls debugScene;
// TODO: add unit tests for the shear matrix function

struct DepthProbleSampler {
	GLuint pbo;

	Grid *grid;
	
	float maxDepth;	

	int numDirections;
	std::vector<Vector3f> directions;

	// size: grid.count * numDirections

	typedef GLfloat DepthSample;
	DepthSample* depthSamples;

	int layerSize;
	int volumeSize;

	/*
	float getDepthSample( const Vector3i &index3 ) {
		return depthSamples[ grid->getIndex( index3 ) ] / double( ~0u ) * maxDepth;
	}*/

	GLuint getDepthSample( int directionIndex, const Vector3i &index3 ) {
		return depthSamples[ grid->getIndex( index3 ) + grid->size.prod() * directionIndex ];
	}

	void init() {
		numDirections = directions.size();

		glGenBuffers( 1, &pbo );
		glPixelStorei( GL_PACK_ALIGNMENT, 1 );

		layerSize = sizeof( DepthSample ) * grid->size.head<2>().prod();
		volumeSize = layerSize * grid->size.z();
		glNamedBufferDataEXT( pbo, volumeSize * numDirections, nullptr, GL_DYNAMIC_READ );

		depthSamples = (DepthSample*) glMapNamedBufferEXT( pbo, GL_READ_ONLY );
	}

	static Matrix4f createUniformShearMatrix( const Vector2f &size, const float maxZ, const Vector2f &zStep ) {
		const Vector2f halfSize = size / 2;
		return createShearProjectionMatrix( -halfSize, halfSize, 0.0, maxZ, zStep );
	}

	void sample() {
		glUnmapNamedBufferEXT( pbo );

		int maxTextureSize = 4096;
		BOOST_VERIFY( grid->size.maxCoeff() <= maxTextureSize );

		// create depth renderbuffers and framebuffer objects for each layer
		int numBuffers = grid->size.z() * numDirections;

		std::unique_ptr<GLuint[]> renderBuffers( new GLuint[numBuffers] );
		std::unique_ptr<GLuint[]> fbos( new GLuint[numBuffers] );
				
		glGenFramebuffers( numBuffers, fbos.get() );
		glGenRenderbuffers( numBuffers, renderBuffers.get() );
		
		glDrawBuffer( GL_NONE );

		BOOST_VERIFY( boost::algorithm::all_of( directions, []( const Vector3f &v ) { return abs( v.z() ) > 0.1; } ) );

		const Vector3f minCorner = grid->getPosition( Vector3i( 0,0,0 ) );
		const Vector3f maxCorner = grid->getPosition( grid->size );

		glBindBuffer( GL_PIXEL_PACK_BUFFER, pbo );
		
		glPushAttrib(GL_VIEWPORT_BIT);
		glViewport(0, 0, grid->size.x(), grid->size.y()); 
		
		for( int directionIndex = 0 ; directionIndex < numDirections ; ++directionIndex ) {
			const Vector3f &direction = directions[directionIndex] / directions[directionIndex].z();
			
			const float depthScale = direction.norm();
			
			// set the projection matrix
			glMatrixMode( GL_PROJECTION );
			const float shearedMaxDepth = maxDepth / depthScale;
			glLoadMatrix( createShearProjectionMatrix( minCorner.head<2>(), maxCorner.head<2>(), 0, shearedMaxDepth, direction.head<2>() ) );
			
			glMatrixMode( GL_MODELVIEW );

			for( int i = 0 ; i < grid->size[2] ; ++i ) {
				glBindFramebuffer( GL_FRAMEBUFFER, fbos[i] );
				glBindRenderbuffer( GL_RENDERBUFFER, renderBuffers[i] );
				glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, grid->size.x(), grid->size.y() );
				glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffers[i] );

				glClear( GL_DEPTH_BUFFER_BIT );	

				glLoadIdentity();
				glScalef( 1.0, 1.0, direction.z() > 0 ? -1.0 : 1.0 );
				glTranslatef( 0.0, 0.0, -(grid->resolution * i + grid->offset.z()) );

				debugScene.render();

				glReadPixels( 0, 0, grid->size.x(), grid->size.y(), GL_DEPTH_COMPONENT, GL_FLOAT, (GLvoid*) (i * layerSize + directionIndex * volumeSize) );
			}
		}

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glPopAttrib();

		glDrawBuffer( GL_BACK );

		glDeleteFramebuffers( grid->size.z(), fbos.get() );
		glDeleteRenderbuffers( grid->size.z(), renderBuffers.get() );

		depthSamples = (DepthSample*) glMapNamedBufferEXT( pbo, GL_READ_ONLY );
	}
};

#include "niven.Core.Core.h"

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
	debugScene.drawBox( Vector3f::Constant(8), false, true );
	debugScene.end();

	Grid grid( Vector3i( 4, 4, 4 ), Vector3f( 0.0, 0.0, 0.0 ), 1.0 );

	DepthProbleSampler sampler;
	sampler.grid = &grid;
	sampler.directions.push_back( Vector3f::UnitZ() );
	sampler.directions.push_back( Vector3f( 1.0, 0.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( -1.0, 0.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( 0.0, 1.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( 0.0, -1.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( 1.0, 1.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( 1.0, -1.0, 1.0 ) );
	sampler.maxDepth = 256.0;

	sampler.init();
	sampler.sample();

	DebugRender::CombinedCalls depthInfo;
	depthInfo.begin();
	depthInfo.setColor( Vector3f( 1.0, 1.0, 1.0 ) );
	for( int directionIndex = 0 ; directionIndex < sampler.numDirections ; ++directionIndex ) {
		const Vector3f &direction = sampler.directions[directionIndex].normalized();

		for( int i = 0 ; i < grid.count ; i++ ) {
			depthInfo.setPosition( grid.getPosition( grid.getIndex3(i) ) );
			depthInfo.drawVector( sampler.depthSamples[i + directionIndex * grid.count ] * sampler.maxDepth * direction );
		}
	}
	depthInfo.end();

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

		debugScene.render();
		depthInfo.render();

		// End the current frame and display its contents on screen
		window.display();
	}

};