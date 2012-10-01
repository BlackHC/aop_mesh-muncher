#pragma once

#include <boost/timer/timer.hpp>
#include "boost/tuple/tuple_comparison.hpp"
#include "boost/range/algorithm/stable_sort.hpp"
#include "boost/range/algorithm_ext/push_back.hpp"

#include <math.h>

#include "optixProgramInterface.h"

#include <Eigen/Eigen>

typedef OptixProgramInterface::Probe Probe;
typedef OptixProgramInterface::ProbeContext ProbeContext;

/*
struct ProbeDatabase {
	int hitDistanceBuckets[OptixProgramInterface::numProbeSamples+1];
	std::vector< Probe > probes;
	std::vector< ProbeContext > probeContexts;
};*/

/*
struct NestedOutput {
	static int currentIndentation;
	int indentation;

	template< typename T >
	std::ostream & operator << ( const T & value ) {

	}
};*/


#include <autoTimer.h>

#include <boost/lexical_cast.hpp>

// TODO: append _full [9/26/2012 kirschan2]
__forceinline__ bool probeContext_lexicographicalLess( const ProbeContext &a, const ProbeContext &b ) {
	return
		boost::make_tuple( a.hitCounter, a.distance, a.color.x, a.color.y, a.color.z )
		<
		boost::make_tuple( b.hitCounter, b.distance, b.color.x, a.color.y, a.color.z );
}

#if 0
inline bool probeContext_lexicographicalLess_startWithDistance( const ProbeContext &a, const ProbeContext &b ) {
	return
		boost::make_tuple( a.distance, a.color.x, a.color.y, a.color.z )
		<
		boost::make_tuple( b.distance, b.color.x, a.color.y, a.color.z );
}
#endif

//////////////////////////////////////////////////////////////////////////
#include <sort_permute_iter.h>
#include "boost/range/algorithm/sort.hpp"
#include <boost/iterator/counting_iterator.hpp>

struct ProbeDataset {
	std::vector< Probe > probes;
	std::vector< ProbeContext > probeContexts;

	std::vector<int> hitCounterLowerBounds;

	ProbeDataset() {}

	ProbeDataset( ProbeDataset &&other ) : probes( std::move( other.probes ) ), probeContexts( std::move( other.probeContexts ) ) {}

	ProbeDataset & operator = ( ProbeDataset && other ) {
		probes = std::move( other.probes );
		probeContexts = std::move( other.probeContexts );

		return *this;
	}

	ProbeDataset clone() const {
		ProbeDataset cloned;
		cloned.probes = probes;
		cloned.probeContexts = probeContexts;
		cloned.hitCounterLowerBounds = hitCounterLowerBounds;
		return cloned;
	}

	int size() const {
		return (int) probes.size();
	}

	void sort();

	void setHitCounterLowerBounds();

	typedef std::pair< int, int > IntRange;
	IntRange getOcclusionRange( int level ) const {
		return std::make_pair( hitCounterLowerBounds[level], hitCounterLowerBounds[ level + 1 ] );
	}

	// [leftLevel, rightLevel] (ie inclusive!)
	IntRange getOcclusionRange( int leftLevel, int rightLevel ) const {
		return std::make_pair( hitCounterLowerBounds[leftLevel], hitCounterLowerBounds[ rightLevel + 1 ] );
	}

	static ProbeDataset merge( const ProbeDataset &first, const ProbeDataset &second );
	static ProbeDataset mergeMultiple( const std::vector< ProbeDataset* > &datasets);
private:
	// better error messages than with boost::noncopyable
	ProbeDataset( const ProbeDataset &other );
	ProbeDataset & operator = ( const ProbeDataset &other );
};

#if 0

struct CandidateFinderInterface {
	typedef int Id;
	typedef std::vector<Id> Ids;

	struct Query {
		typedef std::shared_ptr<Query> Ptr;

		typedef std::pair< float, Id > WeightedId;
		typedef std::vector<Id> WeightedIds;

		virtual void setQueryDataset( ProbeDataset &&dataset ) = 0;
		virtual void execute() = 0;
		virtual WeightedIds getCandidates() = 0;
	};

	// not a shared pointer because it could contain a lot of data and we do not want that dangling around (better crash)
	virtual Query* createQuery() = 0;

	virtual void addDataset( Id id, ProbeDataset &&dataset ) = 0;
	virtual void integrateDatasets() = 0;

	virtual void loadCache( const char *filename ) = 0;
	virtual void storeCache( const char *filename ) = 0;
};

#endif

struct ProbeContextTolerance {
	// tolerance means: |x-y| < tolerance, then match

	float occusionTolerance;
	float colorTolerance;
	// this should be the resolution of the probes
	float distanceTolerance;

	void setDefault() {
		occusionTolerance = 0.125;
		colorTolerance = 0.25;
		distanceTolerance = 0.25;
	}
};

struct CandidateFinder {
	typedef int Id;
	typedef std::vector<Id> Ids;

	struct Query;

	std::shared_ptr<Query> createQuery() {
		return std::shared_ptr< Query >( new Query( this ) );
	}

	struct Query {
		typedef std::shared_ptr<Query> Ptr;

		struct MatchInfo {
			int id;
			int numMatches;
		};
		typedef std::vector<MatchInfo> MatchInfos;

		void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
			probeContextTolerance = pct;
		}

		void setQueryDataset( ProbeDataset &&dataset ) {
			this->dataset = std::move( dataset );
		}

		void execute() {
			if( !matchInfos.empty() ) {
				throw std::logic_error( "matchInfos is not empty!" );
			}

			processDataset();

			// NOTE: this can be easily parallelized
			for( int id = 0 ; id < parent->idDatasets.size() ; id++ ) {
				MatchInfo result = matchAgainst( id );
				if( result.numMatches > 0 ) {
					matchInfos.push_back( result );
				}
			}
		}

		const MatchInfos & getCandidates() const {
			return matchInfos;
		}

	protected:
		typedef ProbeDataset::IntRange IntRange;
		// first is idDataset, second is the query dataset
		typedef std::pair< IntRange, IntRange > OverlappedRange;

#if 0
		struct ProbeMatch {
			optix::float3 position;
			float radius;
		};
#endif

		MatchInfo matchAgainst( int id ) {
			AUTO_TIMER_FOR_FUNCTION( "id = " + boost::lexical_cast<std::string>( id ) );

			// idea:
			//	use a binary search approach to generate only needed subranges

			// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
			const ProbeDataset &idDataset = parent->idDatasets[id].mergedDataset;

			// we can compare the different occlusion ranges against each other, after including the tolerance

			// TODO: is it better to make both ranges about equally big or not?
			// assuming that the query set is smaller, we enlarge it, to have less items to sort then vice-versa
			// we could determine this at runtime...
			// if( idDatasets.size() > dataset.size() ) {...} else {...}
			int occlusionTolerance = int( OptixProgramInterface::numProbeSamples * probeContextTolerance.occusionTolerance + 0.5 );

			// TODO: use a stack allocated array here? [9/27/2012 kirschan2]

			// only second needs to be sorted
			std::vector< OverlappedRange > overlappedRanges;
			overlappedRanges.reserve( OptixProgramInterface::numProbeSamples );
			for( int occulsionLevel = 0 ; occulsionLevel <= OptixProgramInterface::numProbeSamples ; occulsionLevel++ ) {
				const int leftToleranceLevel = std::max( 0, occulsionLevel - occlusionTolerance );
				const int rightToleranceLevel = std::min( occulsionLevel + occlusionTolerance, OptixProgramInterface::numProbeSamples );

				const ProbeDataset::IntRange idRange = idDataset.getOcclusionRange( occulsionLevel );
				const ProbeDataset::IntRange queryRange = idDataset.getOcclusionRange( leftToleranceLevel, rightToleranceLevel );

				// is one of the ranges empty? if so, we don't need to check it at all
				if( idRange.first == idRange.second || queryRange.first == idRange.second ) {
					continue;
				}

				// store the range for later
				overlappedRanges.push_back( std::make_pair( idRange, queryRange ) );
			}

			MatchInfo matchInfo;
			// TODO: add constructor [9/27/2012 kirschan2]
			matchInfo.id = id;
			matchInfo.numMatches = 0;

			for( auto rangePair = overlappedRanges.begin() ; rangePair != overlappedRanges.end() ; ++rangePair ) {
				matchOverlappedRanges( idDataset, *rangePair, matchInfo );
			}

			return matchInfo;
		}

		void matchOverlappedRanges( const ProbeDataset &idDataset, const OverlappedRange &overlappedRange, MatchInfo &matchInfo ) {
			// TODO: brute force vs sorting -- use both depending on range size [9/27/2012 kirschan2]
			for( int refIndex = overlappedRange.first.first ; refIndex < overlappedRange.first.second ; ++refIndex ) {
				const ProbeContext &refContext = idDataset.probeContexts[ refIndex ];

				for( int queryIndex = overlappedRange.second.first ; queryIndex < overlappedRange.second.second ; ++queryIndex ) {
					const ProbeContext &queryContext = dataset.probeContexts[ queryIndex ];

					if( matchDistanceAndColor( refContext, queryContext ) ) {
						matchInfo.numMatches++;
					}
				}
			}
		}

		__forceinline__ bool matchDistanceAndColor( const ProbeContext &refContext, const ProbeContext &queryContext ) {
			if( fabs( refContext.distance - queryContext.distance ) > probeContextTolerance.distanceTolerance ) {
				return false;
			}

			Eigen::Vector3i colorDistance(
				refContext.color.x - queryContext.color.x,
				refContext.color.y - queryContext.color.y,
				refContext.color.z - queryContext.color.z
			);
			// TODO: cache the last value? [10/1/2012 kirschan2]
			if( colorDistance.squaredNorm() > probeContextTolerance.colorTolerance * probeContextTolerance.colorTolerance * (1<<16) ) {
				return false;
			}
			return true;
		}

#if 0
		void sortSubRangeIndirectlyByDistance( const IntRange &range, const ProbeDataset &dataset, std::vector< int > sortedIndices ) {
			sortedIndices.reserve( range.second - range.first );
			std::copy(
				boost::make_counting_iterator( range.first ),
				boost::make_counting_iterator( range.second ),
				std::back_inserter( sortedIndices ) );

			boost::stable_sort( sortedIndices, [dataset] ( int a, int b ) {
				return probeContext_lexicographicalLess_startWithDistance( dataset.probeContexts[ a ], dataset.probeContexts[ b ] );
			} );
		}
#endif

		void processDataset() {
			AUTO_TIMER_FOR_FUNCTION();

			dataset.sort();
			dataset.setHitCounterLowerBounds();
		}

	protected:
		const CandidateFinder *parent;

		ProbeDataset dataset;

		ProbeContextTolerance probeContextTolerance;

		MatchInfos matchInfos;

	private:
		Query( const CandidateFinder *parent ) : parent( parent ) {}

		friend std::shared_ptr<Query> CandidateFinder::createQuery();
	};

	void reserveIds( Id maxId ) {
		idDatasets.resize( maxId + 1 );
	}

	void addDataset( Id id, ProbeDataset &&dataset ) {
		idDatasets[ id ].insertQueue.emplace_back( std::move( dataset ) );
	}

	void integrateDatasets() {
		for( auto idDataset = idDatasets.begin() ; idDataset != idDatasets.end() ; ++idDataset ) {
			idDataset->processQueue();
		}
	}

	// see candidateFinderCache.cpp
	bool loadCache( const char *filename );
	void storeCache( const char *filename );

public:
	struct IdDatasets {
		std::vector<ProbeDataset> insertQueue;
		ProbeDataset mergedDataset;

		IdDatasets() {}
		IdDatasets( IdDatasets &&other ) : insertQueue( std::move( other.insertQueue ) ), mergedDataset( std::move( other.mergedDataset ) ) {}
		IdDatasets & operator = ( IdDatasets &&other ) {
			insertQueue = std::move( other.insertQueue );
			mergedDataset = std::move( other.mergedDataset );
			return *this;
		}

		void processQueue() {
			if( insertQueue.empty() ) {
				return;
			}

			{
				AUTO_TIMER_DEFAULT( "sort queue");
				int totalCount = 0;

				for( auto dataset = insertQueue.begin() ; dataset != insertQueue.end() ; ++dataset ) {
					dataset->sort();

					// for debug outputs
					totalCount += dataset->size();
				}

				std::cerr << autoTimer.indentation() << "processed queue of " << totalCount << " probes\n";
			}

			if( insertQueue.size() == 1 ) {
				mergedDataset = ProbeDataset::merge( mergedDataset, insertQueue[0] );
			}
			else {
				// create a pointer vector with all datasets
				std::vector< ProbeDataset * > datasets;
				datasets.reserve( 1 + insertQueue.size() );

				datasets.push_back( &mergedDataset );
				for( auto dataset = insertQueue.begin() ; dataset != insertQueue.end() ; ++dataset ) {
					datasets.push_back( &*dataset );
				}

				// merge them all
				mergedDataset = std::move( ProbeDataset::mergeMultiple( datasets ) );

				// reset the queue
				insertQueue.clear();
			}

			// set the hitcounter bounds
			mergedDataset.setHitCounterLowerBounds();
		}

	private:
		// better error messages than with boost::noncopyable
		IdDatasets( const IdDatasets &other );
		IdDatasets & operator = ( const IdDatasets &other );
	};

private:
	std::vector<IdDatasets> idDatasets;
};
