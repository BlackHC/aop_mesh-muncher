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

struct SimpleProbeDataset;
struct ProbeDataset;

namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, SimpleProbeDataset &value );
	template< typename Writer >
	void write( Writer &writer, const SimpleProbeDataset &value );

	template< typename Reader >
	void read( Reader &reader, ProbeDataset &value );
	template< typename Writer >
	void write( Writer &writer, const ProbeDataset &value );
}

#include <autoTimer.h>

#include <boost/lexical_cast.hpp>

// TODO: append _full [9/26/2012 kirschan2]
__forceinline__ bool probeContext_lexicographicalLess( const ProbeContext &a, const ProbeContext &b ) {
	return
		boost::make_tuple( a.hitCounter, a.distance, a.color.x, a.color.y, a.color.z )
		<
		boost::make_tuple( b.hitCounter, b.distance, b.color.x, a.color.y, a.color.z );
}

inline bool probeContext_lexicographicalLess_startWithDistance( const ProbeContext &a, const ProbeContext &b ) {
	return
		boost::make_tuple( a.distance, a.color.x, a.color.y, a.color.z )
		<
		boost::make_tuple( b.distance, b.color.x, a.color.y, a.color.z );
}

//////////////////////////////////////////////////////////////////////////
#include <sort_permute_iter.h>
#include "boost/range/algorithm/sort.hpp"
#include <boost/iterator/counting_iterator.hpp>

struct RawProbeDataset {
	std::vector< Probe > probes;
	std::vector< ProbeContext > probeContexts;

	RawProbeDataset() {}

	RawProbeDataset( std::vector< Probe > &&probes, std::vector< ProbeContext > &&probeContexts ) :
		probes( std::move( probes ) ),
		probeContexts( std::move( probeContexts ) )
	{
	}

	RawProbeDataset( RawProbeDataset &&other ) :
		probes( std::move( other.probes ) ),
		probeContexts( std::move( other.probeContexts ) )
	{
	}

	RawProbeDataset clone() const {
		RawProbeDataset cloned;
		cloned.probes = probes;
		cloned.probeContexts = probeContexts;
		return cloned;
	}

	int size() const {
		return (int) probes.size();
	}

	void sort();

private:
	// better error messages than with boost::noncopyable
	RawProbeDataset( const RawProbeDataset &other );
	RawProbeDataset & operator = ( const RawProbeDataset &other );
};

// no further preprocessed information
// invariant: sorted
struct SimpleProbeDataset {
	SimpleProbeDataset() {}

	SimpleProbeDataset( RawProbeDataset &&other ) :
		probes( std::move( other.probes ) ),
		probeContexts( std::move( other.probeContexts ) )
	{
		sort();
	}

	SimpleProbeDataset( SimpleProbeDataset &&other ) :
		probes( std::move( other.probes ) ),
		probeContexts( std::move( other.probeContexts ) )
	{
	}

	SimpleProbeDataset & operator = ( SimpleProbeDataset &&other ) {
		probes = std::move( other.probes );
		probeContexts = std::move( other.probeContexts );

		return *this;
	}

	SimpleProbeDataset clone() const {
		SimpleProbeDataset cloned;
		cloned.probes = probes;
		cloned.probeContexts = probeContexts;
		return cloned;
	}

	int size() const {
		return (int) probes.size();
	}

	static SimpleProbeDataset merge( const SimpleProbeDataset &first, const SimpleProbeDataset &second );
	static SimpleProbeDataset mergeMultiple( const std::vector< const SimpleProbeDataset* > &datasets);

	SimpleProbeDataset subSet( const std::pair< int, int > &range ) const;

	const std::vector< Probe > &getProbes() const {
		return probes;
	}

	const std::vector< ProbeContext > &getProbeContexts() const {
		return probeContexts;
	}

private:
	void sort();

	std::vector< Probe > probes;
	std::vector< ProbeContext > probeContexts;

	template< typename Reader >
	friend void Serializer::read( Reader &reader, SimpleProbeDataset &value );
	template< typename Writer >
	friend void Serializer::write( Writer &writer, const SimpleProbeDataset &value );

private:
	// better error messages than with boost::noncopyable
	SimpleProbeDataset( const SimpleProbeDataset &other );
	SimpleProbeDataset & operator = ( const SimpleProbeDataset &other );
};

// this dataset creates auxiliary structures automatically
// invariant: sorted and hitCounterLowerBounds is correctly set
struct ProbeDataset {
	SimpleProbeDataset data;

	const std::vector< Probe > &getProbes() const {
		return data.getProbes();
	}

	const std::vector< ProbeContext > &getProbeContexts() const {
		return data.getProbeContexts();
	}

	std::vector<int> hitCounterLowerBounds;

	ProbeDataset() {}

	ProbeDataset( SimpleProbeDataset &&other ) :
		data( std::move( other ) ),
		hitCounterLowerBounds()
	{
		setHitCounterLowerBounds();
	}


	ProbeDataset( ProbeDataset &&other ) :
		data( std::move( other.data ) ),
		hitCounterLowerBounds( std::move( other.hitCounterLowerBounds ) )
	{
	}

	ProbeDataset & operator = ( ProbeDataset && other ) {
		data = std::move( other.data );
		hitCounterLowerBounds = std::move( other.hitCounterLowerBounds );

		return *this;
	}

	ProbeDataset clone() const {
		ProbeDataset cloned;
		cloned.data = data.clone();
		cloned.hitCounterLowerBounds = hitCounterLowerBounds;
		return cloned;
	}

	int size() const {
		return (int) data.size();
	}

	typedef std::pair< int, int > IntRange;
	IntRange getOcclusionRange( int level ) const {
		return std::make_pair( hitCounterLowerBounds[level], hitCounterLowerBounds[ level + 1 ] );
	}

	// [leftLevel, rightLevel] (ie inclusive!)
	IntRange getOcclusionRange( int leftLevel, int rightLevel ) const {
		return std::make_pair( hitCounterLowerBounds[leftLevel], hitCounterLowerBounds[ rightLevel + 1 ] );
	}

private:
	void setHitCounterLowerBounds();

	template< typename Reader >
	friend void Serializer::read( Reader &reader, ProbeDataset &value );
	template< typename Writer >
	friend void Serializer::write( Writer &writer, const ProbeDataset &value );

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

struct ProbeDatabase {
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

			MatchInfo( int id = -1 ) : id( id ), numMatches() {}
		};
		typedef std::vector<MatchInfo> MatchInfos;

		void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
			probeContextTolerance = pct;
		}

		void setQueryDataset( SimpleProbeDataset &&dataset ) {
			this->dataset = std::move( dataset );
		}

		void execute() {
			if( !matchInfos.empty() ) {
				throw std::logic_error( "matchInfos is not empty!" );
			}

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
			// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
			const ProbeDataset &idDataset = parent->idDatasets[id].mergedDataset;

			if( idDataset.size() == 0 ) {
				return MatchInfo( id );
			}

			AUTO_TIMER_FOR_FUNCTION( "id = " + boost::lexical_cast<std::string>( id ) );

			// idea:
			//	use a binary search approach to generate only needed subranges

			// we can compare the different occlusion ranges against each other, after including the tolerance

			// TODO: is it better to make both ranges about equally big or not?
			// its better they are equal

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
				const ProbeDataset::IntRange queryRange = dataset.getOcclusionRange( leftToleranceLevel, rightToleranceLevel );

				// is one of the ranges empty? if so, we don't need to check it at all
				if( idRange.first == idRange.second || queryRange.first == queryRange.second ) {
					continue;
				}

				// store the range for later
				overlappedRanges.push_back( std::make_pair( idRange, queryRange ) );
			}

			MatchInfo matchInfo( id );
			for( auto rangePair = overlappedRanges.begin() ; rangePair != overlappedRanges.end() ; ++rangePair ) {
				matchOverlappedRanges( idDataset, *rangePair, matchInfo );
			}

			return matchInfo;
		}

		// only second needs to be sorted
		void matchOverlappedRanges( const ProbeDataset &idDataset, const OverlappedRange &overlappedRange, MatchInfo &matchInfo ) {
			// assert: the range is not empty

			// sort the ranges into two new vectors
			// idea: use a global scratch space to avoid recurring allocations?
			SimpleProbeDataset scratch = dataset.data.subSet( overlappedRange.second );

			const int querySize = scratch.size();
			int queryIndex = 0;

			const int endRefIndex = overlappedRange.first.second;
			int refIndex = overlappedRange.first.first;

			ProbeContext queryContext = scratch.getProbeContexts()[ queryIndex ];

			for( ; queryIndex < querySize - 1 ; queryIndex++ ) {
				const ProbeContext nextQueryContext = scratch.getProbeContexts()[ queryIndex + 1 ];
				int nextStartRefIndex = refIndex;

				const float minDistance = queryContext.distance - probeContextTolerance.distanceTolerance;
				const float maxDistance = queryContext.distance + probeContextTolerance.distanceTolerance;
				const float minNextDistance = nextQueryContext.distance - probeContextTolerance.distanceTolerance;

				// assert: minDistance <= minNextDistance

				for( ; refIndex < endRefIndex ; refIndex++ ) {
					const ProbeContext &refContext = idDataset.getProbeContexts()[ refIndex ];

					// distance too small?
					if( refContext.distance < minDistance ) {
						// then the next one is too far away as well
						nextStartRefIndex = refIndex + 1;
						continue;
					}

					// if nextQueryContext can't use this probe, the next ref context might be the first one it likes
					if( refContext.distance < minNextDistance ) {
						// set it to the next ref context
						nextStartRefIndex = refIndex + 1;
					}
					// else:
					//  nextStartRefIndex points to the first ref content the next query context might match

					// are we past our interval
					if( refContext.distance > maxDistance ) {
						// enough for this probe, do the next
						break;
					}

					if( matchColor( refContext, queryContext ) ) {
						matchInfo.numMatches++;
					}
				}

				queryContext = nextQueryContext;
				refIndex = nextStartRefIndex;
			}

			// process the last element
			for( ; refIndex < endRefIndex ; refIndex++ ) {
				const ProbeContext &refContext = idDataset.getProbeContexts()[ refIndex ];

				// distance too small?
				if( refContext.distance < queryContext.distance - probeContextTolerance.distanceTolerance ) {
					continue;
				}
				if( refContext.distance > queryContext.distance + probeContextTolerance.distanceTolerance ) {
					// done
					break;
				}

				if( matchColor( refContext, queryContext ) ) {
					matchInfo.numMatches++;
				}
			}
		}

		__forceinline__ bool matchColor( const ProbeContext &refContext, const ProbeContext &queryContext ) {
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

	protected:
		const ProbeDatabase *parent;

		ProbeDataset dataset;

		ProbeContextTolerance probeContextTolerance;

		MatchInfos matchInfos;

	private:
		Query( const ProbeDatabase *parent ) : parent( parent ) {}

		friend std::shared_ptr<Query> ProbeDatabase::createQuery();
	};

	void reserveIds( Id maxId ) {
		idDatasets.resize( maxId + 1 );
	}

	void addDataset( Id id, SimpleProbeDataset &&dataset ) {
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
		std::vector<SimpleProbeDataset> insertQueue;
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

			/*{
				AUTO_TIMER_DEFAULT( "sort queue");
				int totalCount = 0;

				for( auto dataset = insertQueue.begin() ; dataset != insertQueue.end() ; ++dataset ) {
					dataset->sort();

					// for debug outputs
					totalCount += dataset->size();
				}

				std::cerr << autoTimer.indentation() << "processed queue of " << totalCount << " probes\n";
			}*/

			if( insertQueue.size() == 1 ) {
				mergedDataset = std::move( SimpleProbeDataset::merge( mergedDataset.data, insertQueue[0] ) );
			}
			else {
				// create a pointer vector with all datasets
				std::vector< const SimpleProbeDataset * > datasets;
				datasets.reserve( 1 + insertQueue.size() );

				datasets.push_back( &mergedDataset.data );
				for( auto dataset = insertQueue.begin() ; dataset != insertQueue.end() ; ++dataset ) {
					datasets.push_back( &*dataset );
				}

				// merge them all
				mergedDataset = std::move( SimpleProbeDataset::mergeMultiple( datasets ) );

				// reset the queue
				insertQueue.clear();
			}
		}

	private:
		// better error messages than with boost::noncopyable
		IdDatasets( const IdDatasets &other );
		IdDatasets & operator = ( const IdDatasets &other );
	};

private:
	std::vector<IdDatasets> idDatasets;
};
