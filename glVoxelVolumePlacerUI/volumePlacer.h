#pragma once

#include <iostream>
#include <utility>
#include <vector>

#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/size.hpp>

#include <boost/timer/timer.hpp>

#include <Eigen/Eigen>

#include "contextHelper.h"

#include <cstdlib>
#include <cmath>

#include "grid.h"

#include "colorAndDepthSampler.h"
#include <memory>
#include "cielab.h"

using namespace Eigen;

const float CIELAB_ColorStep = 30.0; // ~ difference between 1.0 and 0.5 channel intensity

// z: 9, x: 9, y: 8
const Vector3f neighborOffsets[] = {
	// first z, then x, then y

	// z
	Vector3f( 0, 0, 1 ), Vector3f( 0, 0, -1 ), Vector3f( -1, 0, -1 ),
	Vector3f( 1, 0, -1 ), Vector3f( -1, 0, 1 ),  Vector3f( 1, 0, 1 ),
	Vector3f( 0, 1, -1 ), Vector3f( 0, -1, -1 ), Vector3f( -1, 1, -1 ),
	// x
	Vector3f( 1, 0, 0 ), Vector3f( -1, 0, 0  ), Vector3f( 1, 1, -1 ),
	Vector3f( 1, -1, 1 ), Vector3f( 1, 1, 1 ), Vector3f( 1, -1, -1 ),
	Vector3f( -1, 1, 0 ), Vector3f( 1, 1, 0 ), Vector3f( -1, -1, -1 ),
	// y
	Vector3f( 0, 1, 0 ), Vector3f( 0, -1, 0 ), Vector3f( -1, -1, 0 ), Vector3f( 1, -1, 0 ),
	Vector3f( -1, -1, 1 ), Vector3f( 0, -1, 1 ),  Vector3f( -1, 1, 1 ), Vector3f( 0, 1, 1 )
};

template< typename Data, typename OrientedGrid = SimpleOrientedGrid >
class DataGrid : public boost::noncopyable {
	OrientedGrid grid;
	std::unique_ptr<Data[]> data;

public:
	DataGrid() {}

	// move constructor
	DataGrid( DataGrid &&other ) : grid( std::move( other.grid ) ), data( std::move( other.data ) ) {}

	DataGrid & operator = ( DataGrid &&other ) {
		grid = std::move( other.grid );
		data = std::move( other.data );

		return *this;
	}

	DataGrid( const OrientedGrid &grid ) {
		reset( grid );
	}

	void reset( const OrientedGrid &grid ) {
		this->grid = grid;
		data.reset( new Data[ grid.count ] );
	}

	const OrientedGrid & getGrid() const {
		return grid;
	}

	typename OrientedGrid::Iterator getIterator() const {
		return OrientedGrid::Iterator( grid );
	}

	Data & operator[] ( const int index ) {
		return data[ index ];
	}

	Data & operator[] ( const Eigen::Vector3i &index3 ) {
		return data[ grid.getIndex( index3 ) ];
	}

	const Data & operator[] ( const int index ) const {
		return data[ index ];
	}

	const Data & operator[] ( const Eigen::Vector3i &index3 ) const {
		return data[ grid.getIndex( index3 ) ];
	}

	Data & get( const int index ) {
		return data[ index ];
	}

	Data & get( const Eigen::Vector3i &index3 ) {
		return data[ grid.getIndex( index3 ) ];
	}

	const Data & get( const int index ) const {
		return data[ index ];
	}

	const Data & get( const Eigen::Vector3i &index3 ) const {
		return data[ grid.getIndex( index3 ) ];
	}
	
	Data tryGet( const Eigen::Vector3i &index3 ) const {
		if( grid.isValid( index3 ) ) {
			return get( index3 );
		}
		else {
			return Data();
		}
	}
};

struct Voxel {
	Eigen::Vector3f color;
	int weight; // traversal count
	int count; // target count

	Voxel() : weight( 0 ), color( Vector3f::Zero() ), count( 0 ) {}
};

typedef DataGrid<Voxel, SubOrientedGrid> VoxelGrid;

void voxelize( const Samples &probeGrid, VoxelGrid &voxelGrid, float maxDistance ) {
	// use the same resolution for now
	voxelGrid.reset( probeGrid.getGrid().getExpandedGrid( maxDistance ) );

	const float resolution = probeGrid.getGrid().getResolution();
	for( auto iterator = probeGrid.getGrid().getIterator() ; iterator.hasMore() ; ++iterator ) {
		const Vector3i &index3 = iterator.getIndex3();
		
		voxelGrid[index3].weight += 1000;

		for( int directionIndex = 0 ; directionIndex < boost::size( neighborOffsets ) ; ++directionIndex ) {
			// its important that all direction coeffs are either 0, 1 or -1
			const Vector3i direction = neighborOffsets[ directionIndex ].cast<int>();

			const auto &sample = probeGrid.getSample( *iterator, directionIndex );
		
			const int numSteps = int( sample.depth / direction.cast<float>().norm() / resolution + 0.5 );
			for( int i = 1 ; i <= numSteps - 1 ; ++i ) {
				const Vector3i target = index3 + i * direction;
				if( voxelGrid.getGrid().isValid(target) ) {
					voxelGrid[target].weight++;
				}
			}

			if( sample.depth == maxDistance ) {
				continue;
			}
			const Vector3i target = index3 + numSteps * direction;
			if( voxelGrid.getGrid().isValid(target) ) {
				auto &voxel = voxelGrid[target];
				voxel.color += ColorConversion::RGB_to_CIELab( Vector3f( sample.color.r / 255.0, sample.color.g / 255.0, sample.color.b / 255.0 ) );
				voxel.weight++;
				voxel.count++;
			}
		}
	}

	for( auto iterator = voxelGrid.getIterator() ; iterator.hasMore() ; ++iterator ) {
		auto &voxel = voxelGrid[ *iterator ];
		if( voxel.count ) {
			voxel.color /= voxel.count;
		}
	}
}

void sampleProbes( Samples &samples, std::function<void()> renderSceneCallback, float maxDistance ) {
	boost::timer::auto_cpu_timer timer;

	VolumeSampler<Samples::View> sampler;
	sampler.samplesView.samples = &samples;
	sampler.grid = &samples.getGrid();
	
	sampler.directions[0].assign( &neighborOffsets[0], &neighborOffsets[9]);
	sampler.directions[1].assign( &neighborOffsets[9], &neighborOffsets[18]);
	sampler.directions[2].assign( &neighborOffsets[18], &neighborOffsets[26]);
	
	sampler.init();
	sampler.maxDepth = maxDistance;

	sampler.sample( renderSceneCallback );
}

typedef DataGrid<int, SubOrientedGrid> SumGrid;

// build a sum/potential grid over voxels[ * ].count > 0
void buildSumGrid( const VoxelGrid &voxels, SumGrid &sumGrid ) {
	sumGrid.reset( voxels.getGrid() );
	SimpleIndexer3 originIndexer( voxels.getGrid().getSize() );

	// along x
	for( int z = 0 ; z < originIndexer.size.z() ; ++z ) {
		for( int y = 0 ; y < originIndexer.size.y() ; ++y ) {
			int sum = 0;
			for( int x = 0 ; x < originIndexer.size.x() ; ++x ) {
				const int index = originIndexer.getIndex( Eigen::Vector3i( x, y, z ) );				
				bool voxelSet = voxels[ index ].count > 0;
				sum = sumGrid[ index ] = sum + voxelSet ? 1 : 0;				
			}
		}
	}
	// along y
	for( int z = 0 ; z < originIndexer.size.z() ; ++z ) {
		for( int x = 0 ; x < originIndexer.size.x() ; ++x ) {
			int sum = sumGrid[ originIndexer.getIndex( Eigen::Vector3i( x, 0, z ) ) ];
			for( int y = 1 ; y < originIndexer.size.y() ; ++y ) {
				const int index = originIndexer.getIndex( Eigen::Vector3i( x, y, z ) );

				sum = sumGrid[ index ] += sum;				
			}
		}
	}
	// along z
	for( int y = 0 ; y < originIndexer.size.y() ; ++y ) {
		for( int x = 0 ; x < originIndexer.size.x() ; ++x ) {
			int sum = sumGrid[ originIndexer.getIndex( Eigen::Vector3i( x, y, 0 ) ) ];

			for( int z = 1 ; z < originIndexer.size.z() ; ++z ) {	
				const int index = originIndexer.getIndex( Eigen::Vector3i( x, y, z ) );

				sum = sumGrid[ index ] += sum;				
			}
		}
	}
}

struct ObjectDatabase : boost::noncopyable {
	struct InstanceInfo : boost::noncopyable {
		VoxelGrid voxelGrid;
		int solidCount;
		int dontCares;
		int totalWeight;

		InstanceInfo( InstanceInfo &&other ) : voxelGrid( std::move( other.voxelGrid ) ), solidCount( other.solidCount ), dontCares( other.dontCares ), totalWeight( other.totalWeight ) {}
		InstanceInfo & operator = ( InstanceInfo &&other ) {
			voxelGrid = std::move( other.voxelGrid );
			solidCount = other.solidCount;
			dontCares = other.dontCares;
			totalWeight = other.totalWeight;
			return *this;
		}

		InstanceInfo( VoxelGrid &&voxels ) : voxelGrid( std::move( voxels ) ), solidCount( 0 ), dontCares( 0 ), totalWeight( 0 ) {
			for( int i = 0 ; i < voxelGrid.getGrid().count ; ++i ) {
				auto &sample = voxelGrid[ i ];

				if( sample.weight ) {
					totalWeight += sample.weight;

					if( sample.count ) {
						solidCount++;
					}
				}
				else {
					dontCares++;
				}
			}
		}
	};

	struct TemplateInfo : boost::noncopyable {
		DataGrid< float, SimpleOrientedGrid > mergedWeights; // > 0 for solid voxels, < 0 for empty voxels
		std::vector< InstanceInfo > instances;

		int cares;
		float totalMergedWeight;

		TemplateInfo() : cares( 0 ), totalMergedWeight( 0 ) {}

		TemplateInfo( TemplateInfo &&other ) : mergedWeights( std::move( other.mergedWeights ) ), instances( std::move( instances ) ), cares( other.cares ), totalMergedWeight( other.totalMergedWeight ) {}
		TemplateInfo & operator = ( TemplateInfo &&other ) {
			mergedWeights = std::move( other.mergedWeights );
			instances = std::move( other.instances );
			cares = other.cares;
			totalMergedWeight = other.totalMergedWeight;
			return *this;
		}

		void mergeInstances() {
			cares = 0;
			totalMergedWeight = 0.0f;

			if( instances.empty() ) {
				return;
			}

			// assert: all grids are equal
			mergedWeights.reset( instances[0].voxelGrid.getGrid().getGridAtOrigin() );
			DataGrid< float, SimpleOrientedGrid > tempGrid( instances[0].voxelGrid.getGrid().getGridAtOrigin() );

			int count = mergedWeights.getGrid().count;
			for( int i = 0 ; i < count ; ++i ) {
				float mergedWeight = 0;
				for( int j = 0 ; j < instances.size() ; ++j ) {
					const auto &sample = instances[j].voxelGrid[ i ];
					if( sample.count > 0 ) {
						mergedWeight += sample.weight / float( instances[j].totalWeight );
					}
					else {
						mergedWeight -= sample.weight / float( instances[j].totalWeight );
					}
				}
				tempGrid[i] = mergedWeight;
			}

			// blur
			// TODO: use a Gaussian filter..
			const float attenuation = 1.0 / 27.0;
			for( auto iterator = mergedWeights.getIterator() ; iterator.hasMore() ; ++iterator ) {
				float averagedNeighborhood = tempGrid[ *iterator ] * 2.0 / 3.0;
				for( int i = 0 ; i < boost::size( neighborOffsets ) ; i++ ) {
					const Eigen::Vector3i neighborIndex3 = iterator.getIndex3() + neighborOffsets[i].cast<int>();
					averagedNeighborhood += tempGrid.tryGet( neighborIndex3 ) * attenuation / 3.0;
				}
				mergedWeights[ *iterator ] = averagedNeighborhood;
				if( mergedWeights[ *iterator ] != 0 ) {
					cares++;
				}
				totalMergedWeight += abs( mergedWeights[ *iterator ] );
			}
		}
	};

	std::vector< TemplateInfo > templates;

	ObjectDatabase() {}
	ObjectDatabase( ObjectDatabase &&other ) : templates( std::move( other.templates ) ) {}

	ObjectDatabase & operator = ( ObjectDatabase &&other ) {
		templates = std::move( other.templates );
		return *this;
	}

	void init( int numIds ) {
		templates.resize( numIds );
	}

	void addInstance( int id, VoxelGrid &&context ) {
		BOOST_ASSERT( id < templates.size() );
		templates[id].instances.emplace_back( std::move( InstanceInfo( std::move( context ) ) ) );
	}

	void finishInstance( int id ) {
		templates[id].mergeInstances();
	}

	void finish() {
		for( int id = 0 ; id < templates.size() ; ++id ) {
			finishInstance( id );
		}
	}

	struct TemplateSuggestions {
		int id;

		float bestScore;

		struct Suggestion {
			Eigen::Vector3f position;
			float score;
		};

		std::vector< Suggestion > suggestions;
	};

	typedef std::vector<TemplateSuggestions> Suggestions;

	Suggestions findSuggestions( const VoxelGrid &targetVolume ) {
		Suggestions results;

		SimpleOrientedGrid originIndexer = targetVolume.getGrid().getGridAtOrigin();

		/*SumGrid sumGrid;
		buildSumGrid( targetVolume, sumGrid );*/

		// loop through all templates and find the best matches
		for( int id = 0 ; id < templates.size() ; ++id ) {
			const auto &template_ = templates[id];

			if( template_.cares == 0 ) {
				continue;
			}
			
			Eigen::Vector3i remainingSize = targetVolume.getGrid().getSize() - template_.mergedWeights.getGrid().getSize();
			if( (remainingSize.array() <= 0).any() ) {
				continue;
			}

			auto searchIterator = originIndexer.getSubIterator( Eigen::Vector3i::Zero(), remainingSize );
			auto templateIterator = template_.mergedWeights.getIterator();

			TemplateSuggestions templateSuggestions;
			templateSuggestions.id = id;
			templateSuggestions.bestScore = 0.0;

			for( ; searchIterator.hasMore() ; ++searchIterator ) {
				float score = 0;

				auto targetIterator = originIndexer.getSubIterator( searchIterator.getIndex3(), searchIterator.getIndex3() + template_.mergedWeights.getGrid().getSize() );
				auto templateIterator = template_.mergedWeights.getIterator();
				for( ; templateIterator.hasMore() ; ++templateIterator, ++targetIterator ) {
					// solid
					score += (2*int(targetVolume[ *targetIterator ].count > 0)-1) * template_.mergedWeights[ *templateIterator ];
				}

				if( score > templateSuggestions.bestScore) {
					templateSuggestions.bestScore = score;
				}

				TemplateSuggestions::Suggestion suggestion;
				suggestion.score = score / template_.totalMergedWeight;
				suggestion.position = originIndexer.getInterpolatedPosition( searchIterator.getIndex3().cast<float>() + 0.5 * template_.mergedWeights.getGrid().getSize().cast<float>() );
				templateSuggestions.suggestions.push_back( suggestion );
			}

			boost::sort( templateSuggestions.suggestions, [] (const TemplateSuggestions::Suggestion &a, const TemplateSuggestions::Suggestion &b) { return a.score > b.score; } );

			results.push_back( templateSuggestions );
		}

		boost::sort( results, [] (const TemplateSuggestions &a, const TemplateSuggestions &b) { return a.bestScore > b.bestScore; } );
		
		return results;
	}
};

#if 0
void printCandidates( std::ostream &out, const ObjectDatabase::TemplateSuggestions &candidates ) {
	out << candidates.size() << " candidates\n";
	for( int i = 0 ; i < candidates.size() ; ++i ) {
		out << "Weight: " << candidates[i].score << "\t\tId: " << candidates[i].id << "\n";
	}
}
#endif 