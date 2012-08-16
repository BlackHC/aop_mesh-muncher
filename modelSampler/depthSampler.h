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

class DepthSamples {
	const OrientedGrid *grid;

	// xyz
	std::unique_ptr<float[]> depthSamples;
	int numDirections;

public:
	void init( const OrientedGrid *grid, int numDirections ) {
		this->grid = grid;
		this->numDirections = numDirections;

		depthSamples.reset( new float[ grid->count * numDirections ] );
	}

	const OrientedGrid &getGrid() const {
		return *grid;
	}

	float getSample( int index, int directionIndex ) const {
		return depthSamples[ index * numDirections + directionIndex ];
	}

	float &sample( int index, int directionIndex ) {
		return depthSamples[ index * numDirections + directionIndex ];
	}

	const float * getSampleBegin( int index ) const {
		return &depthSamples[ index * numDirections ];
	}

	const float * getSampleEnd( int index ) const {
		return &depthSamples[ (index + 1) * numDirections ];
	}
};

#include <omp.h>
#include <stdio.h>
void report_num_threads(int level)
{
	printf("Level %d: %i/%d\n",
			level, omp_get_thread_num(), omp_get_num_threads());
}

struct DepthSampler {
	GLuint pbo;

	const OrientedGrid *grid;

	float maxDepth;

	int numDirections;
	// sorted by main axis xyz, yzx, zxy
	// these are index directions (ie they need to be transformed by the oriented grid to obtain scene directions)
	std::vector<Eigen::Vector3f> directions[3];

	// size: grid.count * numDirections

	typedef GLfloat DepthSample;

	DepthSamples depthSamples;

	DepthSample getMappedDepthSample( const float *mappedDepthSamples, int index, int directionIndex ) const {
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

	void rearrangeMappedDepthSamples( const float *mappedDepthSamples ) {
		boost::timer::auto_cpu_timer timer;

		/*
		semi sequential writes:
			8.4		0.10
		semi semi sequential writes:
			9.x		0.18
		sequential writes:
			9.04	0.218
		sequential reads:
			14		0.1

		so the simplest code yields the best results
		*/
#if 1
		// semi sequential writes
		int directionOffset[3] = { 0, directions[0].size(), numDirections - directions[2].size() };

		omp_set_nested( true );
#pragma omp parallel for num_threads(3)
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };
			Indexer3 permutedIndexer = Indexer3::fromPermuted( *grid, permutation );

#pragma omp parallel for num_threads(9)
			for( int i = 0 ; i < directions[mainAxis].size() ; ++i ) {
				const int directionIndex = directionOffset[mainAxis] + i;
				for( int sampleIndex = 0 ; sampleIndex < grid->count ; ++sampleIndex ) {
					// we iterate sequentially over the result samples
					const Eigen::Vector3i targetIndex3 = grid->getIndex3( sampleIndex );
					// index into the source data (we permute the coordinates)
					const Eigen::Vector3i sourceIndex3 = permute( targetIndex3, permutation );
					const int sourceIndex = permutedIndexer.getIndex( sourceIndex3 );

					depthSamples.sample( sampleIndex, directionIndex ) = getMappedDepthSample( mappedDepthSamples, sourceIndex, directionIndex ) * maxDepth;
				}
			}
		}
#elif 0
		// semi semi sequential writes
		int directionIndex = 0;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			// 1. moved up to avoid recalculation
			Indexer3 permutedIndexer = Indexer3::fromPermuted( *grid, permutation );

			int startDirectionIndex = directionIndex;
			for( int sampleIndex = 0 ; sampleIndex < grid->count ; ++sampleIndex ) {
				directionIndex = startDirectionIndex;
				for( int i = 0 ; i < directions[mainAxis].size() ; ++i, ++directionIndex ) {
					// we iterate sequentially over the result samples
					const Eigen::Vector3i targetIndex3 = grid->getIndex3( sampleIndex );
					// index into the source data (we permute the coordinates)
					const Eigen::Vector3i sourceIndex3 = permute( targetIndex3, permutation );
					const int sourceIndex = permutedIndexer.getIndex( sourceIndex3 );

					depthSamples.sample( sampleIndex, directionIndex ) = getMappedDepthSample( mappedDepthSamples, sourceIndex, directionIndex ) * maxDepth;
				}
			}
		}
#elif 0
		// sequential writes
		int permutations[3][3] = { {0,1,2}, {1,2,0}, {2,1,0} };
		Indexer3 permutedIndexers[3] = { Indexer3::fromPermuted( *grid, permutations[0] ), Indexer3::fromPermuted( *grid, permutations[1] ), Indexer3::fromPermuted( *grid, permutations[2] ) };

		for( int sampleIndex = 0 ; sampleIndex < grid->count ; ++sampleIndex ) {
			int directionIndex = 0;
			for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
				for( int i = 0 ; i < directions[mainAxis].size() ; ++i, ++directionIndex ) {
					// we iterate sequentially over the result samples
					const Eigen::Vector3i targetIndex3 = grid->getIndex3( sampleIndex );
					// index into the source data (we permute the coordinates)
					const Eigen::Vector3i sourceIndex3 = permute( targetIndex3, permutations[mainAxis] );
					const int sourceIndex = permutedIndexers[mainAxis].getIndex( sourceIndex3 );

					depthSamples.sample( sampleIndex, directionIndex ) = getMappedDepthSample( mappedDepthSamples, sourceIndex, directionIndex ) * maxDepth;
				}
			}
		}
#else
		// sequential reads
		int directionIndex = 0;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			// 1. moved up to avoid recalculation
			Indexer3 permutedIndexer = Indexer3::fromPermuted( *grid, permutation );

			for( int i = 0 ; i < directions[mainAxis].size() ; ++i, ++directionIndex ) {
				for( int sampleIndex = 0 ; sampleIndex < grid->count ; ++sampleIndex ) {
					// we iterate sequentially over the source samples (that are stored in different order)
					const Eigen::Vector3i permutedIndex3 = permutedIndexer.getIndex3( sampleIndex );
					// index into the result buffer (we unpermute the coordinates)
					const Eigen::Vector3i targetIndex3 = permute_reverse( permutedIndex3, permutation );
					const int targetIndex = grid->getIndex( targetIndex3 );

					depthSamples.sample( targetIndex, directionIndex ) = getMappedDepthSample( mappedDepthSamples, sampleIndex, directionIndex ) * maxDepth;
				}
			}
		}
#endif 
	}

	void sample( std::function<void()> renderSceneCallback ) {
		int maxTextureSize = 4096;
		BOOST_VERIFY( grid->size.maxCoeff() <= maxTextureSize );

		// create depth renderbuffers and framebuffer objects for each layer
		int numBuffers = grid->size.z() * directions[0].size() + grid->size.x() * directions[1].size() + grid->size.y() * directions[2].size();

		std::unique_ptr<GLuint[]> renderBuffers( new GLuint[numBuffers] );
		std::unique_ptr<GLuint[]> fbos( new GLuint[numBuffers] );

		glGenFramebuffers( numBuffers, fbos.get() );
		glGenRenderbuffers( numBuffers, renderBuffers.get() );

		glDrawBuffer( GL_NONE );

		size_t totalSizeInMB = (sizeof( DepthSample ) * grid->count * numDirections + (1<<20)-1) >> 20;
		size_t currentSizeinMB = 0;

		int directionIndex = 0;
		DepthSample *offset = nullptr;
		for( int mainAxis = 0 ; mainAxis < 3 ; ++mainAxis ) {
			const std::vector<Eigen::Vector3f> &subDirections = directions[mainAxis];

			int permutation[3] = { mainAxis, (mainAxis + 1) % 3, (mainAxis + 2) % 3 };

			BOOST_VERIFY( boost::algorithm::all_of( subDirections, [&permutation]( const Eigen::Vector3f &v ) { return abs( v[permutation[2]] ) > 0.1; } ) );

			OrientedGrid permutedGrid = OrientedGrid::from( *grid, permutation );

			glBindBuffer( GL_PIXEL_PACK_BUFFER, pbo );

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
					glBindFramebuffer( GL_FRAMEBUFFER, fbos[i] );
					glBindRenderbuffer( GL_RENDERBUFFER, renderBuffers[i] );
					glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, permutedGrid.size[0], permutedGrid.size[1] );
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffers[i] );

					glClear( GL_DEPTH_BUFFER_BIT );	

					glLoadIdentity();
					
					// looking down the negative z axis by default --- so flip if necessary (this changes winding though!!!)
					glScalef( 1.0, 1.0, (permutedDirection.z() > 0 ? -1.0 : 1.0) * permutedGrid.getDirection( Eigen::Vector3f::UnitZ() ).norm() );

					// pixel alignment and "layer selection"
					glTranslatef( 0.5, 0.5, -i );

					// ie "position to permuted index"
					Eigen::glMultMatrix( permutedGrid.positionToIndex );

					renderSceneCallback();
					
					glReadPixels( 0, 0, permutedGrid.size[0], permutedGrid.size[1], GL_DEPTH_COMPONENT, GL_FLOAT, (GLvoid*) offset );

					offset += permutedGrid.size.head<2>().prod();

					size_t newSizeInMB = ((char*)offset - (char*)nullptr) >> 20;
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
		glDeleteRenderbuffers( numBuffers, renderBuffers.get() );

		const DepthSample *mappedDepthSamples = (const DepthSample*) glMapNamedBufferEXT( pbo, GL_READ_ONLY );
		rearrangeMappedDepthSamples( mappedDepthSamples );
		glUnmapNamedBufferEXT( pbo );
	}
};
