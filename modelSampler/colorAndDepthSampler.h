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
#include <boost/timer/timer.hpp>

#include <omp.h>

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
	const SimpleIndexMapping3 *grid;
	
	// xyz
	std::unique_ptr<Sample[]> samples;
	int numDirections;

public:
	void init( const SimpleIndexMapping3 *grid, int numDirections ) {
		this->grid = grid;
		this->numDirections = numDirections;

		samples.reset( new Sample[ grid->count * numDirections ] );
	}

	const SimpleIndexMapping3 & getGrid() const {
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

	struct View {
		Samples *samples;

		inline void putSample( int index, int directionIndex, const Color4ub &color, const float depth ) {
			Sample &sample = samples->sample( index, directionIndex );
			sample.depth = depth;
			sample.color = color;
		}
	};
};

template<typename SamplesView = Samples::View>
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

	const SimpleIndexMapping3 *grid;

	float maxDepth;

	int numDirections;
	// sorted by main axis xyz, yzx, zxy
	// these are index directions (ie they need to be transformed by the oriented grid to obtain scene directions)
	std::vector<Eigen::Vector3f> directions[3];

	// size: grid.count * numDirections

	SamplesView samplesView;

	const ColorSample & getMappedColorSample( const ColorSample *mappedColorSamples, int index, int directionIndex ) const {
		return mappedColorSamples[ directionIndex * grid->count + index ];
	}

	const DepthSample & getMappedDepthSample( const DepthSample *mappedDepthSamples, int index, int directionIndex ) const {
		return mappedDepthSamples[ directionIndex * grid->count + index ];
	}

	void init() {
		numDirections = directions[0].size() + directions[1].size() + directions[2].size();

		glPixelStorei( GL_PACK_ALIGNMENT, 1 );
		size_t numSamples = grid->count * numDirections;
		depthPBO.init( numSamples );
		colorPBO.init( numSamples );
	}

	void mergeSamples( const ColorSample *mappedColorSamples, const DepthSample *mappedDepthSamples ) {
		// semi sequential writes
		int directionOffset[3] = { 0, directions[0].size(), numDirections - directions[2].size() };

		omp_set_nested( true );
#pragma omp parallel for num_threads(3)
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			// 1. moved up to avoid recalculation
			SimpleIndexer3 permutedIndexer = grid->SimpleIndexer3::permuted( permutation );

#pragma omp parallel for num_threads(9)
			for( int i = 0 ; i < directions[mainAxis].size() ; ++i ) {
				const int directionIndex = directionOffset[mainAxis] + i;
				for( int sampleIndex = 0 ; sampleIndex < grid->count ; ++sampleIndex ) {
					// we iterate sequentially over the result samples
					const Eigen::Vector3i targetIndex3 = grid->getIndex3( sampleIndex );
					// index into the source data (we permute the coordinates)
					const Eigen::Vector3i sourceIndex3 = permute( targetIndex3, permutation );
					const int sourceIndex = permutedIndexer.getIndex( sourceIndex3 );

					samplesView.putSample( 
						sampleIndex, 
						directionIndex, 
						getMappedColorSample( mappedColorSamples, sourceIndex, directionIndex ), 
						getMappedDepthSample( mappedDepthSamples, sourceIndex, directionIndex ) * maxDepth 
					);
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

		size_t totalSizeInMB = ((sizeof( GLfloat ) + sizeof( GLbyte[4] )) * grid->count * numDirections) >> 20;
		size_t currentSizeinMB = 0;

		int directionIndex = 0;
		size_t offset = 0;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			const std::vector<Eigen::Vector3f> &subDirections = directions[mainAxis];

			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			BOOST_VERIFY( boost::algorithm::all_of( subDirections, [&permutation]( const Eigen::Vector3f &v ) { return abs( v[permutation[2]] ) > 0.1; } ) );

			SimpleIndexMapping3 permutedGrid = grid->permuted( permutation );

			glPushAttrib(GL_VIEWPORT_BIT);
			glViewport(0, 0, permutedGrid.size[0], permutedGrid.size[1]); 

			for( int subDirectionIndex = 0 ; subDirectionIndex < directions[mainAxis].size() ; ++subDirectionIndex, ++directionIndex ) {
				const Eigen::Vector3f direction = subDirections[subDirectionIndex];
				const Eigen::Vector3f permutedDirection = permute( direction.normalized(), permutation ) * maxDepth;

				const float resolution = permutedGrid.getDirection( Eigen::Vector3f::UnitZ() ).norm();

				// set the projection matrix
				glMatrixMode( GL_PROJECTION );
				const float shearedMaxDepth = abs( permutedDirection[2] );
				Eigen::glLoadMatrix( createShearProjectionMatrix( Eigen::Vector2f::Zero(), permutedGrid.size.head<2>().cast<float>(), 0, shearedMaxDepth, permutedDirection.head<2>() / shearedMaxDepth / resolution ) );

				for( int i = 0 ; i < permutedGrid.size[2] ; ++i ) {
					glNamedRenderbufferStorageEXT( colorRenderBuffers[i], GL_RGBA8, permutedGrid.size[0], permutedGrid.size[1] );
					glNamedRenderbufferStorageEXT( depthRenderBuffers[i], GL_DEPTH_COMPONENT32F, permutedGrid.size[0], permutedGrid.size[1] );

					glBindFramebuffer( GL_FRAMEBUFFER, fbos[i] );
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBuffers[i] );
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderBuffers[i] );

					glDrawBuffer( GL_COLOR_ATTACHMENT0 );
										
					// TODO: remove | GL_COLOR_BUFFER_BIT again!
					glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );	

					//glMatrixMode( GL_PROJECTION );
					glPushMatrix();

					// looking down the negative z axis by default --- so flip if necessary (this changes winding though!!!)
					glScalef( 1.0, 1.0, (permutedDirection.z() > 0 ? -1.0 : 1.0) * resolution );

					// pixel alignment and "layer selection"
					glTranslatef( 0.5, 0.5, -i );
					//
					// ie "position to permuted index"
					Eigen::glMultMatrix( permutedGrid.positionToIndex );

					glMatrixMode( GL_MODELVIEW );
					glLoadIdentity();

					renderSceneCallback();

					glMatrixMode( GL_PROJECTION );
					glPopMatrix();

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
