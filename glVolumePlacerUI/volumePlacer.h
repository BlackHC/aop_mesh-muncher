#pragma once

#include <iostream>
#include <utility>
#include <vector>

#include <boost/range/algorithm.hpp>

#include <Eigen/Eigen>

#include "contextHelper.h"

#include <cstdlib>
#include <cmath>

#include "grid.h"

#include "depthSampler.h"
#include <memory>

using namespace Eigen;

// z: 6, x: 2, y: 18
const Vector3i neighborOffsets[] = {
	// first z, then x, then y

	// z
	Vector3i( -1, 0, -1 ), Vector3i( 0, 0, -1 ), Vector3i( 1, 0, -1 ), 
	Vector3i( -1, 0, 1 ), Vector3i( 0, 0, 1 ), Vector3i( 1, 0, 1 ), 

	// x
	Vector3i( -1, 0, 0  ), Vector3i( 1, 0, 0 ), 

	// y
	Vector3i( -1, -1, 0 ), Vector3i( 0, -1, 0 ), Vector3i( 1, -1, 0 ),
	Vector3i( -1, 1, 0 ), Vector3i( 0, 1, 0 ), Vector3i( 1, 1, 0 ),
	Vector3i( -1, -1, -1 ), Vector3i( 0, -1, -1 ), Vector3i( 1, -1, -1 ),
	Vector3i( -1, 1, -1 ), Vector3i( 0, 1, -1 ), Vector3i( 1, 1, -1 ),
	Vector3i( -1, -1, 1 ), Vector3i( 0, -1, 1 ), Vector3i( 1, -1, 1 ),
	Vector3i( -1, 1, 1 ), Vector3i( 0, 1, 1 ), Vector3i( 1, 1, 1 ),

/*	// z = 0
	Vector3i( -1, 0, 0  ), Vector3i( 1, 0, 0 ), 
	Vector3i( -1, -1, 0 ), Vector3i( 0, -1, 0 ), Vector3i( 1, -1, 0 ),
	Vector3i( -1, 1, 0 ), Vector3i( 0, 1, 0 ), Vector3i( 1, 1, 0 ),
	// z = -1
	Vector3i( -1, 0, -1 ), Vector3i( 0, 0, -1 ), Vector3i( 1, 0, -1 ), 
	Vector3i( -1, -1, -1 ), Vector3i( 0, -1, -1 ), Vector3i( 1, -1, -1 ),
	Vector3i( -1, 1, -1 ), Vector3i( 0, 1, -1 ), Vector3i( 1, 1, -1 ),
	// z = 1
	Vector3i( -1, 0, 1 ), Vector3i( 0, 0, 1 ), Vector3i( 1, 0, 1 ), 
	Vector3i( -1, -1, 1 ), Vector3i( 0, -1, 1 ), Vector3i( 1, -1, 1 ),
	Vector3i( -1, 1, 1 ), Vector3i( 0, 1, 1 ), Vector3i( 1, 1, 1 ),*/
};

struct UnorderedDistanceContext {
	static const int numSamples = 26;
	static Vector3f directions[numSamples];
	float sortedDistances[numSamples];
	float distances[numSamples];

	static void setDirections() {
		for( int i = 0 ; i < numSamples ; i++ ) {
			directions[i] = neighborOffsets[i].cast<float>();
		}
	}

	void fill( const DepthSamples &samples, const int index ) {
		std::copy( samples.getSampleBegin( index ), samples.getSampleEnd( index ), distances );
		boost::range::copy( distances, sortedDistances );
		std::sort( sortedDistances, sortedDistances + numSamples );
	}

	double calculateAverage() const {
		double average = 0.;
		for( int index = 0 ; index < numSamples ; ++index ) {
			average += sortedDistances[index];
		}
		average /= numSamples;
		return average;
	}

	void normalizeWithAverage() {
		double average = calculateAverage();
		for( int index = 0 ; index < numSamples ; ++index ) {
			sortedDistances[index] /= average;
		}
	}

	static double compare( const UnorderedDistanceContext &a, const UnorderedDistanceContext &b, const float maxDistance ) {
		// use L2 norm because it accentuates big differences better than L1
		double norm = 0.;
		for( int index = 0 ; index < numSamples ; ++index ) {
			double delta = std::min( maxDistance, a.sortedDistances[index] ) - std::min( b.sortedDistances[index], maxDistance );
			norm = std::max( norm, std::abs(delta) );
		}
		return norm;
	}
};

Vector3f UnorderedDistanceContext::directions[UnorderedDistanceContext::numSamples];

struct ProbeMatchSettings : AsExecutionContext<ProbeMatchSettings> {
	float maxDistance;
	float maxDelta;

	void setDefault() {
		maxDistance = 128.0;
		maxDelta = 1.0;
	}
};

// TODO: rename to InstanceProbe or InstanceSample
struct Probe {
	typedef UnorderedDistanceContext DistanceContext;
	DistanceContext distanceContext;

	static bool match( const Probe &a, const Probe &b ) {
		return DistanceContext::compare( a.distanceContext, b.distanceContext, ProbeMatchSettings::context->maxDistance ) <= ProbeMatchSettings::context->maxDelta;
	}
};

typedef std::vector<Probe> ProbeVector;

namespace Serialize {
	template<typename T>
	void writeTyped( FILE *fileHandle, const T &d ) {
		fwrite( &d, sizeof(T), 1, fileHandle );
	}

	template<typename T>
	void readTyped( FILE *fileHandle, T &d ) {
		fread( &d, sizeof(T), 1, fileHandle );
	}

	template<typename V>
	void writeTyped( FILE *fileHandle, const std::vector<V> &d ) {
		int num = (int) d.size();
		writeTyped( fileHandle, num );
		for( int i = 0 ; i < num ; ++i ) {
			writeTyped( fileHandle, d[i] );
		}
	}

	template<typename V>
	void readTyped( FILE *fileHandle, std::vector<V> &d ) {
		int num;
		readTyped( fileHandle, num );
		d.resize( num );

		for( int i = 0 ; i < num ; ++i ) {
			readTyped( fileHandle, d[i] );
		}
	}
}

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
};

typedef DataGrid<Probe> ProbeGrid;

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

		std::vector< const InstanceProbe * > matches;
		std::vector< std::pair<Eigen::Vector3f, int> > matchesPositionEndOffsets;

		CandidateInfo() : totalMatchCount(0), maxSingleMatchCount(0), score(0) {}

		CandidateInfo( CandidateInfo &&o ) : score( o.score ), totalMatchCount( o.totalMatchCount ), maxSingleMatchCount( o.maxSingleMatchCount ), matches( std::move( o.matches ) ), matchesPositionEndOffsets( std::move( o.matchesPositionEndOffsets ) ) {}
	};

	typedef std::vector<CandidateInfo> CandidateInfos;
	typedef std::vector< std::pair<int, CandidateInfo > > SparseCandidateInfos;

	SparseCandidateInfos findCandidates( const ProbeGrid &probeGrid ) {
		CandidateInfos candidateInfos( numIds );

		std::vector<int> matchCounts(numIds);
		for( Iterator3 iterator = probeGrid.getIterator() ; iterator.hasMore() ; ++iterator ) {
			const Probe &probe = probeGrid[ *iterator ];

			std::fill( matchCounts.begin(), matchCounts.end(), 0 );
		
			for( auto refIterator = probeIdMap.cbegin() ; refIterator != probeIdMap.cend() ; ++refIterator ) {
				if( Probe::match( refIterator->probe, probe ) ) {
					const int id = refIterator->id;
					candidateInfos[id].matches.push_back( &*refIterator );
					matchCounts[id]++;
				}
			}				

			for( int id = 0 ; id < numIds ; ++id ) {
				const int matchCount = matchCounts[id];
				if( matchCount == 0 ) {
					continue;
				}

				CandidateInfo &candidateInfo = candidateInfos[id];

				candidateInfo.totalMatchCount += matchCount;
				candidateInfo.maxSingleMatchCount = std::max( candidateInfo.maxSingleMatchCount, matchCount );

				candidateInfo.matchesPositionEndOffsets.push_back( std::make_pair( probeGrid.getGrid().getPosition( iterator.getIndex3() ), (int) candidateInfo.matches.size() ) );
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
	DepthSampler sampler;
	sampler.grid = &probeGrid.getGrid();
	
	sampler.directions[0].assign( &UnorderedDistanceContext::directions[0], &UnorderedDistanceContext::directions[6]);
	sampler.directions[1].assign( &UnorderedDistanceContext::directions[6], &UnorderedDistanceContext::directions[8]);
	sampler.directions[2].assign( &UnorderedDistanceContext::directions[8], &UnorderedDistanceContext::directions[26]);
	
	sampler.init();
	sampler.maxDepth = maxDistance;

	sampler.sample( renderSceneCallback );

	for( Iterator3 iterator = probeGrid.getIterator() ; iterator.hasMore() ; ++iterator ) {
		Probe &probe = probeGrid[ *iterator ];
		probe.distanceContext.fill( sampler.depthSamples, iterator.getIndex() );
	}
}

void printCandidates( std::ostream &out, const ProbeDatabase::SparseCandidateInfos &candidates ) {
	out << candidates.size() << " candidates\n";
	for( int i = 0 ; i < candidates.size() ; ++i ) {
		out << "Weight: " << candidates[i].second.score << "\t\tId: " << candidates[i].first << "\n";
	}
}