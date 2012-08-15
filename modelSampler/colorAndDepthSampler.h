#pragma once

#include <GL/glew.h>

#include <vector>
#include <memory>
#include <functional>
#include <iostream>

#include <boost/algorithm/cxx11/all_of.hpp>

#include "grid.h"
#include "eigenProjectionMatrices.h"

#include <unsupported/Eigen/OpenGLSupport>
#include <boost/assert.hpp>

__declspec(align(4)) struct Color4ub {
	GLubyte r,g,b;
	char x;
};

class Samples {
public:
	struct Sample {
		float depth;
		Color4ub color;
	};

private:
	const OrientedGrid *grid;
	
	// xyz
	std::unique_ptr<Sample[]> samples;
	int numDirections;

public:
	void init( const OrientedGrid *grid, int numDirections ) {
		this->grid = grid;
		this->numDirections = numDirections;

		samples.reset( new Sample[ grid->count * numDirections ] );
	}

	const OrientedGrid & getGrid() const {
		return *grid;
	}

	const Sample & getSample( int index, int directionIndex ) const {
		return samples[ index * numDirections + directionIndex ];
	}

	Sample & sample( int index, int directionIndex ) {
		return samples[ index * numDirections + directionIndex ];
	}

	const Sample * getSampleBegin( int index ) const {
		return &samples[ index * numDirections ];
	}

	const Sample * getSampleEnd( int index ) const {
		return &samples[ (index + 1) * numDirections ];
	}
};

struct VolumeSampler {
	typedef Samples::Sample Sample;
	typedef Color4ub ColorSample;
	typedef float DepthSample;

	template<typename Value>
	struct ReadOncePBO {
		GLuint handle;

		ReadOncePBO() : handle( 0 ) {}

		ReadOncePBO( size_t numSamples ) {
			init( numSamples );
		}

		void init( size_t numSamples ) {
			glGenBuffers( 1, &handle );

			glNamedBufferDataEXT( handle, numSamples * sizeof( Value ), nullptr, GL_STREAM_READ );
		}

		void reset() {
			if( handle ) {
				glDeleteBuffers( 1, &handle );
				handle = 0;
			}
		}

		~ReadOncePBO() {
			reset();
		}
	};
	ReadOncePBO<DepthSample> depthPBO;
	ReadOncePBO<ColorSample> colorPBO;

	const OrientedGrid *grid;

	float maxDepth;

	int numDirections;
	// sorted by main axis xyz, yzx, zxy
	// these are index directions (ie they need to be transformed by the oriented grid to obtain scene directions)
	std::vector<Eigen::Vector3f> directions[3];

	// size: grid.count * numDirections

	Samples samples;

	const ColorSample & getMappedColorSample( const ColorSample *mappedColorSamples, int index, int directionIndex ) const {
		return mappedColorSamples[ directionIndex * grid->count + index ];
	}

	const DepthSample & getMappedDepthSample( const DepthSample *mappedDepthSamples, int index, int directionIndex ) const {
		return mappedDepthSamples[ directionIndex * grid->count + index ];
	}

	void init() {
		numDirections = directions[0].size() + directions[1].size() + directions[2].size();
		samples.init( grid, numDirections );

		glPixelStorei( GL_PACK_ALIGNMENT, 1 );
		size_t numSamples = grid->count * numDirections;
		depthPBO.init( numSamples );
		colorPBO.init( numSamples );
	}

	void mergeSamples( const ColorSample *mappedColorSamples, const DepthSample *mappedDepthSamples ) {
		int directionIndex = 0;
		// TODO: parallelize this..
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			for( int i = 0 ; i < directions[mainAxis].size() ; ++i, ++directionIndex ) {
				for( int sampleIndex = 0 ; sampleIndex < grid->count ; ++sampleIndex ) {
					const Eigen::Vector3i index3 = grid->getIndex3( sampleIndex );
					const Eigen::Vector3i targetIndex3 = permute( index3, permutation );

					Indexer3 permutedIndexer = Indexer3::fromPermuted( *grid, permutation );
					const int targetIndex = permutedIndexer.getIndex( targetIndex3 );

					Sample &sample = samples.sample( sampleIndex, directionIndex );
					sample.depth = getMappedDepthSample( mappedDepthSamples, targetIndex, directionIndex ) * maxDepth;
					sample.color = getMappedColorSample( mappedColorSamples, targetIndex, directionIndex );
				}
			}
		}		 
	}

	void sample( std::function<void()> renderSceneCallback ) {
		int maxTextureSize = 4096;
		BOOST_VERIFY( grid->size.maxCoeff() <= maxTextureSize );

		// create depth renderbuffers and framebuffer objects for each layer
		int numBuffers = grid->size.z() * directions[0].size() + grid->size.x() * directions[1].size() + grid->size.y() * directions[2].size();

		std::unique_ptr<GLuint[]> depthRenderBuffers( new GLuint[numBuffers] );
		std::unique_ptr<GLuint[]> colorRenderBuffers( new GLuint[numBuffers] );
		std::unique_ptr<GLuint[]> fbos( new GLuint[numBuffers] );

		glGenFramebuffers( numBuffers, fbos.get() );
		glGenRenderbuffers( numBuffers, depthRenderBuffers.get() );
		glGenRenderbuffers( numBuffers, colorRenderBuffers.get() );

		glDrawBuffer( GL_NONE );

		size_t totalSizeInMB = ((sizeof( GLfloat ) + sizeof( GLbyte[3] )) * grid->count * numDirections + (1<<20)-1) >> 20;
		size_t currentSizeinMB = 0;

		int directionIndex = 0;
		size_t offset = 0;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			const std::vector<Eigen::Vector3f> &subDirections = directions[mainAxis];

			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			BOOST_VERIFY( boost::algorithm::all_of( subDirections, [&permutation]( const Eigen::Vector3f &v ) { return abs( v[permutation[2]] ) > 0.1; } ) );

			OrientedGrid permutedGrid = OrientedGrid::from( *grid, permutation );

			glPushAttrib(GL_VIEWPORT_BIT);
			glViewport(0, 0, permutedGrid.size[0], permutedGrid.size[1]); 

			for( int subDirectionIndex = 0 ; subDirectionIndex < directions[mainAxis].size() ; ++subDirectionIndex, ++directionIndex ) {
				const Eigen::Vector3f direction = subDirections[subDirectionIndex];
				const Eigen::Vector3f permutedDirection = permute( direction.normalized(), permutation ) * maxDepth;

				// set the projection matrix
				glMatrixMode( GL_PROJECTION );
				const float shearedMaxDepth = abs( permutedDirection[2] );
				Eigen::glLoadMatrix( createShearProjectionMatrix( Eigen::Vector2f::Zero(), permutedGrid.size.head<2>().cast<float>(), 0, shearedMaxDepth, permutedDirection.head<2>() / shearedMaxDepth ) );

				glMatrixMode( GL_MODELVIEW );

				for( int i = 0 ; i < permutedGrid.size[2] ; ++i ) {
					glNamedRenderbufferStorageEXT( colorRenderBuffers[i], GL_RGBA8, permutedGrid.size[0], permutedGrid.size[1] );
					glNamedRenderbufferStorageEXT( depthRenderBuffers[i], GL_DEPTH_COMPONENT32F, permutedGrid.size[0], permutedGrid.size[1] );

					glBindFramebuffer( GL_FRAMEBUFFER, fbos[i] );
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBuffers[i] );
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderBuffers[i] );

					glDrawBuffer( GL_COLOR_ATTACHMENT0 );
										
					glClear( GL_DEPTH_BUFFER_BIT );	

					glLoadIdentity();
					
					// looking down the negative z axis by default --- so flip if necessary (this changes winding though!!!)
					glScalef( 1.0, 1.0, (permutedDirection.z() > 0 ? -1.0 : 1.0) * permutedGrid.getDirection( Eigen::Vector3f::UnitZ() ).norm() );

					// pixel alignment and "layer selection"
					glTranslatef( 0.5, 0.5, -i );

					// ie "position to permuted index"
					Eigen::glMultMatrix( permutedGrid.positionToIndex );

					renderSceneCallback();

					glReadBuffer( GL_COLOR_ATTACHMENT0 );					

					glBindBuffer( GL_PIXEL_PACK_BUFFER, depthPBO.handle );
					glReadPixels( 0, 0, permutedGrid.size[0], permutedGrid.size[1], GL_DEPTH_COMPONENT, GL_FLOAT, (GLvoid*) (sizeof( GLfloat ) * offset) );
					glBindBuffer( GL_PIXEL_PACK_BUFFER, colorPBO.handle );
					glReadPixels( 0, 0, permutedGrid.size[0], permutedGrid.size[1], GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*) (sizeof( GLbyte[4] ) * offset) );

					offset += permutedGrid.size.head<2>().prod();

					size_t newSizeInMB = (offset * (sizeof( GLfloat ) + sizeof( GLbyte[4] ) )) >> 20;
					if( newSizeInMB != currentSizeinMB ) {
						currentSizeinMB = newSizeInMB;
						std::cout << currentSizeinMB << "/" << totalSizeInMB << std::endl;
					}
				}
			}
			glPopAttrib();
		}

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );

		glDrawBuffer( GL_BACK );

		glDeleteFramebuffers( numBuffers, fbos.get() );
		glDeleteRenderbuffers( numBuffers, colorRenderBuffers.get() );
		glDeleteRenderbuffers( numBuffers, depthRenderBuffers.get() );

		const DepthSample *mappedDepthSamples = (const DepthSample*) glMapNamedBufferEXT( depthPBO.handle, GL_READ_ONLY );
		const ColorSample *mappedColorSamples = (const ColorSample*) glMapNamedBufferEXT( colorPBO.handle, GL_READ_ONLY );
		mergeSamples( mappedColorSamples, mappedDepthSamples );
		glUnmapNamedBufferEXT( depthPBO.handle );
		glUnmapNamedBufferEXT( colorPBO.handle );
	}
};
