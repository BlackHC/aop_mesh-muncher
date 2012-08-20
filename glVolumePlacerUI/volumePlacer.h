#pragma once

#include <iostream>
#include <utility>
#include <vector>

#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <boost/timer/timer.hpp>

#include <Eigen/Eigen>

#include "contextHelper.h"

#include <cstdlib>
#include <cmath>

#include "grid.h"

#include "colorAndDepthSampler.h"
#include <memory>

using namespace Eigen;

struct ProbeSettings : AsExecutionContext<ProbeSettings> {
	float maxDelta;
	float maxDistance;

	void setDefault() {
		maxDelta = 0.0;
		maxDistance = std::numeric_limits<float>::max();
	}
};

// z: 9, x: 9, y: 8
const Vector3i neighborOffsets[] = {
	// first z, then x, then y

	// z
	Vector3i( 0, 0, 1 ), Vector3i( 0, 0, -1 ), Vector3i( -1, 0, -1 ),
	Vector3i( 1, 0, -1 ), Vector3i( -1, 0, 1 ),  Vector3i( 1, 0, 1 ),
	Vector3i( 0, 1, -1 ), Vector3i( 0, -1, -1 ), Vector3i( -1, 1, -1 ),
	// x
	Vector3i( 1, 0, 0 ), Vector3i( -1, 0, 0  ), Vector3i( 1, 1, -1 ),
	Vector3i( 1, -1, 1 ), Vector3i( 1, 1, 1 ), Vector3i( 1, -1, -1 ),
	Vector3i( -1, 1, 0 ), Vector3i( 1, 1, 0 ), Vector3i( -1, -1, -1 ),
	// y
	Vector3i( 0, 1, 0 ), Vector3i( 0, -1, 0 ), Vector3i( -1, -1, 0 ), Vector3i( 1, -1, 0 ),
	Vector3i( -1, -1, 1 ), Vector3i( 0, -1, 1 ),  Vector3i( -1, 1, 1 ), Vector3i( 0, 1, 1 )
};

struct EnvironmentContext {
	struct Sample {
		float depth;
		Vector3f color;
	};

	static const int numSamples = 26;
	static Vector3f directions[numSamples];

	Sample samples[numSamples];
	Sample samplesSortedByDistance[numSamples];
	
	// != global maxDistance
	//float realMaxDistance;
	
	static void setDirections() {
		for( int i = 0 ; i < numSamples ; i++ ) {
			directions[i] = neighborOffsets[i].cast<float>(); 
		}
	}

	static float getDepth( const Sample &sample ) {
		return sample.depth;
	}

	void finish() {
		//boost::range::copy( samples | boost::adaptors::transformed( &getDepth ), sortedDistances );
		boost::range::copy( samples, samplesSortedByDistance );
		std::sort( samplesSortedByDistance, samplesSortedByDistance + numSamples, [] ( const Sample &a, const Sample &b ) { return a.depth < b.depth; } );

		/*int i;
		for( i = numSamples - 1 ; i > 0 ; --i ) {
			if( samplesSortedByDistance[i].depth < ProbeSettings::context->maxDistance - ProbeSettings::context->maxDelta / 2 ) {
				break;
			}
		}
		realMaxDistance = samplesSortedByDistance[i].depth;*/
	}
	
	static bool match( const EnvironmentContext &a, const EnvironmentContext &b, const float maxDelta ) {
#define COLOR_AND_DEPTH_MATCH(i) \
		if( std::abs( a.samplesSortedByDistance[i].depth - b.samplesSortedByDistance[i].depth ) > maxDelta || \
			(a.samplesSortedByDistance[i].color - b.samplesSortedByDistance[i].color).lpNorm<Eigen::Infinity>() > 0.5 ) { \
			return false; \
		}
#define COLOR_MATCH(i) \
	if(	(a.samplesSortedByDistance[i].color - b.samplesSortedByDistance[i].color).lpNorm<Eigen::Infinity>() > 0.5 ) { \
	return false; \
	} 
#define DEPTH_MATCH(i) \
	if( std::abs( a.samplesSortedByDistance[i].depth - b.samplesSortedByDistance[i].depth ) > maxDelta ) { \
	return false; \
	} 

		DEPTH_MATCH(0)
		DEPTH_MATCH( numSamples - 1 )
		/*if( std::abs( a.realMaxDistance - b.realMaxDistance ) > maxDelta ) {
			return false;
		}*/

		for( int i = 1 ; i < numSamples - 1 ; ++i ) {
			//COLOR_MATCH( i )
			DEPTH_MATCH( i )
		}

		return true;
#undef COLOR_MATCH
#undef DEPTH_MATCH
#undef COLOR_AND_DEPTH_MATCH
	}
};

Vector3f EnvironmentContext::directions[EnvironmentContext::numSamples];

// TODO: rename to InstanceProbe or InstanceSample
struct Probe {
	typedef EnvironmentContext DistanceContext;
	DistanceContext distanceContext;

	static bool match( const Probe &a, const Probe &b ) {
		return DistanceContext::match( a.distanceContext, b.distanceContext, ProbeSettings::context->maxDelta );
	}
};

typedef std::vector<Probe> ProbeVector;

template< typename Data >
class DataGrid {
	OrientedGrid grid;
	std::unique_ptr<Data[]> data;

public:
	DataGrid() {}

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

	Iterator3 getIterator() const {
		return Iterator3( grid );
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
};

typedef DataGrid<Probe> ProbeGrid;

struct SamplerProbeView {
	ProbeGrid *probeGrid;

	inline void putSample( int index, int directionIndex, const Color4ub &color, const float depth ) {
		auto &sample = probeGrid->get( index ).distanceContext.samples[ directionIndex ];
		sample.depth = depth;
		sample.color = Vector3f( color.r / 255.0, color.g / 255.0, color.b / 255.0 );
	}	
};

struct InstanceProbe {
	Probe probe;
	int id;
	//Vector3f delta;
	float distance;

	InstanceProbe( const Probe &probe, int id, float distance ) : probe( probe ), id( id ), distance( distance ) {}
};

// TODO: add weighted probes (probe weighted by inverse instance volume to be scale-invariant (more or less))
struct ProbeDatabase {
	int numIds;

	ProbeDatabase() : numIds( 0 ) {}

	void addObjectInstanceToDatabase( const ProbeGrid &probeGrid, int id ) {
		const Vector3f instanceCenter = probeGrid.getGrid().getCenter();

		numIds = std::max( id + 1, numIds );
		idInfos.resize( numIds );
		
		for( Iterator3 iterator = probeGrid.getIterator() ; iterator.hasMore() ; ++iterator ) {
			const Vector3f delta = probeGrid.getGrid().getPosition( iterator.getIndex3() ) - instanceCenter;

			probeIdMap.push_back( InstanceProbe( probeGrid[ *iterator ], id, delta.norm() ) );
		}

		idInfos[id].numObjects++;
		idInfos[id].totalProbeCount += probeGrid.getGrid().count;
	}

	struct CandidateInfo {
		float score;

		// for the score
		int totalMatchCount;
		int maxSingleMatchCount;

		//std::vector< const InstanceProbe * > matches;
		std::vector< float > matchDistances;
		std::vector< std::pair<Eigen::Vector3f, int> > matchesPositionMatchCount;

		CandidateInfo() : totalMatchCount(0), maxSingleMatchCount(0), score(0) {}

		CandidateInfo( CandidateInfo &&o ) : 
			score( o.score ),
			totalMatchCount( o.totalMatchCount ), 
			maxSingleMatchCount( o.maxSingleMatchCount ), 
			/*matches( std::move( o.matches ) ), */
			matchDistances( std::move( o.matchDistances ) ),
			matchesPositionMatchCount( std::move( o.matchesPositionMatchCount ) ) {
		}
	};

	typedef std::vector<CandidateInfo> CandidateInfos;
	typedef std::vector< std::pair<int, CandidateInfo > > SparseCandidateInfos;

	SparseCandidateInfos findCandidates( const ProbeGrid &probeGrid ) {
		CandidateInfos candidateInfos( numIds );

		boost::timer::auto_cpu_timer timer;

#pragma omp parallel num_threads(12)
		{
			CandidateInfos localCandidateInfos( numIds );
			std::vector<int> matchCounts(numIds);
			//for( Iterator3 iterator = probeGrid.getIterator() ; iterator.hasMore() ; ++iterator ) {
#pragma omp for nowait
			for( int index = 0 ; index < probeGrid.getGrid().count ; ++index ) {
				const Probe &probe = probeGrid[ index ];

				std::fill( matchCounts.begin(), matchCounts.end(), 0 );
		
				for( auto refIterator = probeIdMap.cbegin() ; refIterator != probeIdMap.cend() ; ++refIterator ) {
					if( Probe::match( refIterator->probe, probe ) ) {
						const int id = refIterator->id;
						//candidateInfos[id].matches.push_back( &*refIterator );
						localCandidateInfos[id].matchDistances.push_back( refIterator->distance );
						matchCounts[id]++;
					}
				}				

				for( int id = 0 ; id < numIds ; ++id ) {
					const int matchCount = matchCounts[id];
					if( matchCount == 0 ) {
						continue;
					}

					CandidateInfo &candidateInfo = localCandidateInfos[id];

					candidateInfo.totalMatchCount += matchCount;
					candidateInfo.maxSingleMatchCount = std::max( candidateInfo.maxSingleMatchCount, matchCount );

					candidateInfo.matchesPositionMatchCount.push_back( std::make_pair( probeGrid.getGrid().getPosition( probeGrid.getGrid().getIndex3( index ) ), matchCount ) );
				}
			}
#pragma omp critical
			{
				// merge candidate infos
				for( int id = 0 ; id < numIds ; ++id ) {
					candidateInfos[id].maxSingleMatchCount = std::max( candidateInfos[id].maxSingleMatchCount, localCandidateInfos[id].maxSingleMatchCount );
					candidateInfos[id].totalMatchCount += localCandidateInfos[id].totalMatchCount;

					boost::push_back( candidateInfos[id].matchDistances, localCandidateInfos[id].matchDistances );
					boost::push_back( candidateInfos[id].matchesPositionMatchCount, localCandidateInfos[id].matchesPositionMatchCount );
				}
			}
		}

		SparseCandidateInfos results;
		for( int id = 0 ; id < numIds ; ++id ) {
			CandidateInfo &candidateInfo = candidateInfos[id];

			size_t matchCount = candidateInfo.totalMatchCount;
			if( matchCount > 0 ) {
				candidateInfo.score = float(matchCount) / getProbeCountPerInstanceForId(id);

				//typedef decltype(candidateInfo.matches[0]) value_type;
				//std::sort( candidateInfo.matches.begin(), candidateInfo.matches.end(), [](const value_type &a, const value_type b) { return a.second > b.second; } );

				results.push_back( std::make_pair( id, std::move( candidateInfo ) ) );
			}
		}

		std::sort( results.begin(), results.end(), []( const ProbeDatabase::SparseCandidateInfos::value_type &a, const ProbeDatabase::SparseCandidateInfos::value_type &b ) { return a.second.score > b.second.score; } );
		
		return results;
	}

	float getProbeCountPerInstanceForId( int id ) const {
		return idInfos[id].getAverageProbeCount();
	}

	// probe->id
	std::vector<InstanceProbe> probeIdMap;

	struct IdInfo {
		int numObjects;
		int totalProbeCount;

		IdInfo() : numObjects( 0 ), totalProbeCount( 0 ) {}

		float getAverageProbeCount() const {
			return float( totalProbeCount ) / numObjects;
		}
	};

	std::vector<IdInfo> idInfos;
	std::vector<int> instanceProbeCountForId;
};

// TODO: fix the depthUnit hack
void sampleProbes( ProbeGrid &probeGrid, std::function<void()> renderSceneCallback, float maxDistance ) {
	boost::timer::auto_cpu_timer timer;

	VolumeSampler<SamplerProbeView> sampler;
	sampler.samplesView.probeGrid = &probeGrid;
	sampler.grid = &probeGrid.getGrid();
	
	sampler.directions[0].assign( &EnvironmentContext::directions[0], &EnvironmentContext::directions[9]);
	sampler.directions[1].assign( &EnvironmentContext::directions[9], &EnvironmentContext::directions[18]);
	sampler.directions[2].assign( &EnvironmentContext::directions[18], &EnvironmentContext::directions[26]);
	
	sampler.init();
	sampler.maxDepth = maxDistance;

	sampler.sample( renderSceneCallback );

	for( int index = 0 ; index < probeGrid.getGrid().count ; ++index ) {
		Probe &probe = probeGrid[ index ];
		probe.distanceContext.finish();
	}
}

void printCandidates( std::ostream &out, const ProbeDatabase::SparseCandidateInfos &candidates ) {
	out << candidates.size() << " candidates\n";
	for( int i = 0 ; i < candidates.size() ; ++i ) {
		out << "Weight: " << candidates[i].second.score << "\t\tId: " << candidates[i].first << "\n";
	}
}