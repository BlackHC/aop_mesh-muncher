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
		count = size.prod();
	}

	// x, y, z -> |z|y|x|  
	int getIndex( const Vector3i &index3 ) {
		return index3[0] + size[0] * (index3[1] + size[1] * index3[2]);
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

// TODO: make this a template function
// xyz 120 ->yzx
Vector3i permute( const Vector3i &v, int *indirection ) {
	return Vector3i( v[indirection[0]], v[indirection[1]], v[indirection[2]] );
}

Vector3f permute( const Vector3f &v, int *indirection ) {
	return Vector3f( v[indirection[0]], v[indirection[1]], v[indirection[2]] );
}

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
			int indirection[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };
			
			for( int i = 0 ; i < directions[mainAxis].size() ; ++i, ++directionIndex ) {
				for( int sample = 0 ; sample < grid->count ; ++sample ) {
					const Vector3i index3 = grid->getIndex3( sample );
					const Vector3i targetIndex3 = permute( index3, indirection );
					const Vector3i pSize = permute( grid->size, indirection );

					const int targetIndex = targetIndex3[0] + pSize[0] * (targetIndex3[1] + pSize[1] * targetIndex3[2]);
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

		int directionIndex = 0;
		DepthSample *offset = nullptr;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			const std::vector<Vector3f> &subDirections = directions[mainAxis];

			int indirection[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			BOOST_VERIFY( boost::algorithm::all_of( subDirections, [&indirection]( const Vector3f &v ) { return abs( v[indirection[2]] ) > 0.1; } ) );

			const Vector3f minCorner = permute( grid->getPosition( Vector3i( 0,0,0 ) ), indirection );
			const Vector3f maxCorner = permute( grid->getPosition( grid->size ), indirection );
			const Vector3i pSize = permute( grid->size, indirection );

			glBindBuffer( GL_PIXEL_PACK_BUFFER, pbo );
		
			glPushAttrib(GL_VIEWPORT_BIT);
			glViewport(0, 0, pSize.x(), pSize.y()); 
		
			for( int subDirectionIndex = 0 ; subDirectionIndex < directions[mainAxis].size() ; ++subDirectionIndex, ++directionIndex ) {
				const Vector3f direction = subDirections[subDirectionIndex];
				const Vector3f pDirection = permute( direction, indirection ) / direction[indirection[2]];
			
				const float depthScale = pDirection.norm();
			
				// set the projection matrix
				glMatrixMode( GL_PROJECTION );
				const float shearedMaxDepth = maxDepth / depthScale;
				const Vector2f pixelOffset = Vector2f::Constant( grid->resolution / 2 );
				glLoadMatrix( createShearProjectionMatrix( minCorner.head<2>() - pixelOffset, maxCorner.head<2>() - pixelOffset, 0, shearedMaxDepth, pDirection.head<2>() ) );
			
				glMatrixMode( GL_MODELVIEW );

				for( int i = 0 ; i < pSize[2] ; ++i ) {
					glBindFramebuffer( GL_FRAMEBUFFER, fbos[i] );
					glBindRenderbuffer( GL_RENDERBUFFER, renderBuffers[i] );
					glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, pSize.x(), pSize.y() );
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffers[i] );

					glClear( GL_DEPTH_BUFFER_BIT );	

					glLoadIdentity();
					// looking down the negative z axis by default --- so flip if necessary (this changes winding though!!!)
					glScalef( 1.0, 1.0, pDirection.z() > 0 ? -1.0 : 1.0 );
					glTranslatef( 0.0, 0.0, -(grid->resolution * i + grid->offset[ indirection[2] ]) );
					glMultMatrix( (Eigen::Matrix4f() << Vector3f::Unit( indirection[0] ), Vector3f::Unit( indirection[1] ), Vector3f::Unit( indirection[2] ), Vector3f::Zero(), 0,0,0,1.0 ).finished() );

					debugScene.render();

					glReadPixels( 0, 0, pSize.x(), pSize.y(), GL_DEPTH_COMPONENT, GL_FLOAT, (GLvoid*) offset );

					offset += pSize.head<2>().prod();
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
	debugScene.drawBox( Vector3f::Constant(8), false, true );
	debugScene.end();

	Grid grid( Vector3i( 4, 4, 4 ), Vector3f( 0.0, 0.0, 0.0 ), 1.0 );

	DepthProbleSampler sampler;
	sampler.grid = &grid;
	/*sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions.push_back( Vector3f( 1.0, 0.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( -1.0, 0.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( 0.0, 1.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( 0.0, -1.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( 1.0, 1.0, 1.0 ) );
	sampler.directions.push_back( Vector3f( 1.0, -1.0, 1.0 ) );*/
	sampler.directions[0].push_back( Vector3f( 0.3, 0.0, 1.0 ) );
	sampler.directions[0].push_back( Vector3f( -0.3, 0.0, 1.0 ) );
	sampler.directions[1].push_back( Vector3f( 1.0, 0.0, -0.3 ) );
	sampler.directions[1].push_back( Vector3f( 1.0, 0.0, 0.3 ) );
	sampler.directions[2].push_back( Vector3f( 0.0, 1.0, -0.3 ) );
	sampler.directions[2].push_back( Vector3f( 0.0, 1.0, 0.3 ) );

	sampler.maxDepth = 256.0;

	sampler.init();
	sampler.sample();

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	int z = 0;
	IntVariableControl zControl( &z, 0, 3, sf::Keyboard::Numpad7, sf::Keyboard::Numpad1 );

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

				/*for( int i = 0 ; i < grid.count ; i++ ) {
					depthInfo.setPosition( grid.getPosition( grid.getIndex3(i) ) );
					depthInfo.drawVector( sampler.getDepthSample( directionIndex, grid.getIndex3(i) ) * direction );
				}*/
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