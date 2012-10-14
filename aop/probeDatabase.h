#pragma once

#include <boost/timer/timer.hpp>
#include "boost/tuple/tuple_comparison.hpp"
#include "boost/range/algorithm/stable_sort.hpp"
#include "boost/range/algorithm_ext/push_back.hpp"

#include <math.h>

#include "optixProgramInterface.h"

#include <Eigen/Eigen>
#include "boost/container/vector.hpp"
#include "boost/range/algorithm/count.hpp"
#include <boost/dynamic_bitset.hpp>

#include <ppl.h>
#include <concurrent_vector.h>
#include "boost/range/algorithm_ext/erase.hpp"

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

// TODO: add a serializer forward header with macros to declare these functions [10/3/2012 kirschan2]
struct SortedProbeDataset;
struct IndexedProbeDataset;

namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, SortedProbeDataset &value );
	template< typename Writer >
	void write( Writer &writer, const SortedProbeDataset &value );

	template< typename Reader >
	void read( Reader &reader, IndexedProbeDataset &value );
	template< typename Writer >
	void write( Writer &writer, const IndexedProbeDataset &value );
}

#include <autoTimer.h>

#include <boost/lexical_cast.hpp>

// TODO: append _full [9/26/2012 kirschan2]
//////////////////////////////////////////////////////////////////////////
#include <sort_permute_iter.h>
#include "boost/range/algorithm/sort.hpp"
#include <boost/iterator/counting_iterator.hpp>

struct RawProbeDataset {
	typedef OptixProgramInterface::Probe Probe;
	typedef OptixProgramInterface::ProbeContext ProbeContext;

	__forceinline__ bool probeContext_lexicographicalLess( const ProbeContext &a, const ProbeContext &b ) {
		return
			boost::make_tuple( a.hitCounter, a.distance, a.Lab.x, a.Lab.y, a.Lab.z )
			<
			boost::make_tuple( b.hitCounter, b.distance, b.Lab.x, a.Lab.y, a.Lab.z );
	}

	inline bool probeContext_lexicographicalLess_startWithDistance( const ProbeContext &a, const ProbeContext &b ) {
		return
			boost::make_tuple( a.distance, a.Lab.x, a.Lab.y, a.Lab.z )
			<
			boost::make_tuple( b.distance, b.Lab.x, a.Lab.y, a.Lab.z );
	}

	/*struct ProbeContext {

	};*/
	std::vector< Probe > probes;
	std::vector< ProbeContext > probeContexts;

	RawProbeDataset() {}

	RawProbeDataset( const std::vector< OptixProgramInterface::Probe > &probes, std::vector< OptixProgramInterface::ProbeContext > &&probeContexts ) :
		probes( std::move( probes ) ),
		probeContexts( std::move( probeContexts ) )
	{
	}

	RawProbeDataset( RawProbeDataset &&other ) :
		probes( std::move( other.probes ) ),
		probeContexts( std::move( other.probeContexts ) )
	{
	}

	RawProbeDataset & operator = ( RawProbeDataset &&other ) {
		probes = std::move( other.probes );
		probeContexts = std::move( other.probeContexts );

		return *this;
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
struct SortedProbeDataset {
	typedef RawProbeDataset::Probe Probe;
	typedef RawProbeDataset::ProbeContext ProbeContext;

	SortedProbeDataset() {}

	SortedProbeDataset( RawProbeDataset &&other ) :
		data( std::move( other ) )
	{
		data.sort();
	}

	SortedProbeDataset( SortedProbeDataset &&other ) :
		data( std::move( other.data ) )
	{
	}

	SortedProbeDataset & operator = ( SortedProbeDataset &&other ) {
		data = std::move( other.data );

		return *this;
	}

	SortedProbeDataset clone() const {
		SortedProbeDataset cloned;
		cloned.data = data.clone();
		return cloned;
	}

	int size() const {
		return data.size();
	}

	static SortedProbeDataset merge( const SortedProbeDataset &first, const SortedProbeDataset &second );
	static SortedProbeDataset mergeMultiple( const std::vector< const SortedProbeDataset* > &datasets);

	SortedProbeDataset subSet( const std::pair< int, int > &range ) const;

	const std::vector< Probe > &getProbes() const {
		return data.probes;
	}

	const std::vector< ProbeContext > &getProbeContexts() const {
		return data.probeContexts;
	}

private:
	std::vector< Probe > &probes() {
		return data.probes;
	}

	std::vector< ProbeContext > &probeContexts() {
		return data.probeContexts;
	}

	void sort();

	RawProbeDataset data;

	template< typename Reader >
	friend void Serializer::read( Reader &reader, SortedProbeDataset &value );
	template< typename Writer >
	friend void Serializer::write( Writer &writer, const SortedProbeDataset &value );

private:
	// better error messages than with boost::noncopyable
	SortedProbeDataset( const SortedProbeDataset &other );
	SortedProbeDataset & operator = ( const SortedProbeDataset &other );
};

// this dataset creates auxiliary structures automatically
// invariant: sorted and hitCounterLowerBounds is correctly set
struct IndexedProbeDataset {
	typedef SortedProbeDataset::Probe Probe;
	typedef SortedProbeDataset::ProbeContext ProbeContext;

	SortedProbeDataset data;

	const std::vector< Probe > &getProbes() const {
		return data.getProbes();
	}

	const std::vector< ProbeContext > &getProbeContexts() const {
		return data.getProbeContexts();
	}

	std::vector<int> hitCounterLowerBounds;

	IndexedProbeDataset() {}

	IndexedProbeDataset( SortedProbeDataset &&other ) :
		data( std::move( other ) ),
		hitCounterLowerBounds()
	{
		setHitCounterLowerBounds();
	}

	IndexedProbeDataset( IndexedProbeDataset &&other ) :
		data( std::move( other.data ) ),
		hitCounterLowerBounds( std::move( other.hitCounterLowerBounds ) )
	{
	}

	IndexedProbeDataset & operator = ( IndexedProbeDataset && other ) {
		data = std::move( other.data );
		hitCounterLowerBounds = std::move( other.hitCounterLowerBounds );

		return *this;
	}

	IndexedProbeDataset clone() const {
		IndexedProbeDataset cloned;
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
	friend void Serializer::read( Reader &reader, IndexedProbeDataset &value );
	template< typename Writer >
	friend void Serializer::write( Writer &writer, const IndexedProbeDataset &value );

private:
	// better error messages than with boost::noncopyable
	IndexedProbeDataset( const IndexedProbeDataset &other );
	IndexedProbeDataset & operator = ( const IndexedProbeDataset &other );
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
	float colorLabTolerance;
	// this should be the resolution of the probes
	float distanceTolerance;

	void setDefault() {
		occusionTolerance = 0.125;
		colorLabTolerance = 20;
		distanceTolerance = 0.25;
	}
};

struct ProbeDatabase {
	typedef int Id;
	typedef std::vector<Id> Ids;

	void reserveIds( Id maxId ) {
		idDatasets.resize( maxId + 1 );
	}

	void addDataset( Id id, SortedProbeDataset &&dataset ) {
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
		typedef IndexedProbeDataset::Probe Probe;
		typedef IndexedProbeDataset::ProbeContext ProbeContext;

		std::vector<SortedProbeDataset> insertQueue;
		IndexedProbeDataset mergedDataset;

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

			if( insertQueue.size() == 1 ) {
				mergedDataset = std::move( SortedProbeDataset::merge( mergedDataset.data, insertQueue[0] ) );
			}
			else {
				// create a pointer vector with all datasets
				std::vector< const SortedProbeDataset * > datasets;
				datasets.reserve( 1 + insertQueue.size() );

				datasets.push_back( &mergedDataset.data );
				for( auto dataset = insertQueue.begin() ; dataset != insertQueue.end() ; ++dataset ) {
					datasets.push_back( &*dataset );
				}

				// merge them all
				mergedDataset = std::move( SortedProbeDataset::mergeMultiple( datasets ) );

				// reset the queue
				insertQueue.clear();
			}
		}

	private:
		// better error messages than with boost::noncopyable
		IdDatasets( const IdDatasets &other );
		IdDatasets & operator = ( const IdDatasets &other );
	};

	typedef IdDatasets::Probe Probe;
	typedef IdDatasets::ProbeContext ProbeContext;

	struct Query;
	struct FullQuery;

	void reset() {
		int numIds = idDatasets.size();
		idDatasets.clear();
		idDatasets.resize( numIds );
	}

private:
	std::vector<IdDatasets> idDatasets;
};

struct ProbeDatabase::Query {
	typedef std::shared_ptr<Query> Ptr;

	struct MatchInfo {
		int id;
		int numMatches;
		float score;

		float queryMatchPercentage;
		float probeMatchPercentage;

		MatchInfo( int id = -1 ) : id( id ), numMatches(), score(), queryMatchPercentage(), probeMatchPercentage() {}
	};
	typedef std::vector<MatchInfo> MatchInfos;

	Query( const ProbeDatabase &database ) : database( database ) {}

	void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
		probeContextTolerance = pct;
	}

	void setQueryDataset( SortedProbeDataset &&dataset ) {
		this->dataset = std::move( dataset );
	}

	void execute() {
		if( !matchInfos.empty() ) {
			throw std::logic_error( "matchInfos is not empty!" );
		}

		// NOTE: this can be easily parallelized
		matchInfos.resize( database.idDatasets.size() );

		using namespace Concurrency;

		AUTO_TIMER_MEASURE() {
			int logScope = Log::getScope();

			parallel_for< int >(
				0,
				database.idDatasets.size(),
				[&] ( int id ) {
					Log::initThreadScope( logScope, 0 );

					matchInfos[ id ] = matchAgainst( id );
				}
			);
		}

		boost::remove_erase_if( matchInfos, [] ( const MatchInfo &matchInfo ) { return !matchInfo.numMatches; });
	}

	const MatchInfos & getCandidates() const {
		return matchInfos;
	}

protected:
	typedef IndexedProbeDataset::IntRange IntRange;
	typedef std::pair< IntRange, IntRange > OverlappedRange;

	MatchInfo matchAgainst( int id ) {
		// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
		const IndexedProbeDataset &idDataset = database.idDatasets[id].mergedDataset;

		if( idDataset.size() == 0 ) {
			return MatchInfo( id );
		}

		AUTO_TIMER_FOR_FUNCTION( boost::format( "id = %i, %i ref probes (%i query probes)" ) % id % idDataset.size() % dataset.size() );

		// idea:
		//	use a binary search approach to generate only needed subranges

		// we can compare the different occlusion ranges against each other, after including the tolerance

		// TODO: is it better to make both ranges about equally big or not?
		// its better they are equal

		// assuming that the query set is smaller, we enlarge it, to have less items to sort than vice-versa
		// we could determine this at runtime...
		// if( idDatasets.size() > dataset.size() ) {...} else {...}
		const int occlusionTolerance = int( OptixProgramInterface::numProbeSamples * probeContextTolerance.occusionTolerance + 0.5 );

		// TODO: use a stack allocated array here? [9/27/2012 kirschan2]

		std::vector< OverlappedRange > overlappedRanges;
		overlappedRanges.reserve( OptixProgramInterface::numProbeSamples );
		for( int occulsionLevel = 0 ; occulsionLevel <= OptixProgramInterface::numProbeSamples ; occulsionLevel++ ) {
			const IndexedProbeDataset::IntRange queryRange = dataset.getOcclusionRange( occulsionLevel );

			if( queryRange.first == queryRange.second ) {
				continue;
			}

			const int leftToleranceLevel = std::max( 0, occulsionLevel - occlusionTolerance );
			const int rightToleranceLevel = std::min( occulsionLevel + occlusionTolerance, OptixProgramInterface::numProbeSamples );
			for( int idToleranceLevel = leftToleranceLevel ; idToleranceLevel <= rightToleranceLevel ; idToleranceLevel++ ) {
				const IndexedProbeDataset::IntRange idRange = idDataset.getOcclusionRange( idToleranceLevel );

				// is one of the ranges empty? if so, we don't need to check it at all
				if( idRange.first == idRange.second ) {
					continue;
				}

				// store the range for later
				overlappedRanges.push_back( std::make_pair( queryRange, idRange ) );
			}
		}

		using namespace Concurrency;
		boost::dynamic_bitset<> mergedOverlappedProbesMatched( dataset.size() ), mergedPureProbesMatched( idDataset.size() );
		combinable< int > numMatches;

		{
			combinable< boost::dynamic_bitset<> > overlappedProbesMatched, pureProbesMatched;

			//AUTO_TIMER_BLOCK( "matching" ) {
			{
				AUTO_TIMER_FOR_FUNCTION();
				parallel_for_each( overlappedRanges.begin(), overlappedRanges.end(),
					[&] ( const OverlappedRange &rangePair ) {
						overlappedProbesMatched.local().resize( dataset.size() );
						pureProbesMatched.local().resize( idDataset.size() );

						matchSortedRanges(
							dataset.data,
							rangePair.first,
							overlappedProbesMatched.local(),

							idDataset.data,
							rangePair.second,
							pureProbesMatched.local(),

							numMatches.local()
						);
					}
				);
			}
			//AUTO_TIMER_BLOCK( "combining matches" ) {
			{
				AUTO_TIMER_FOR_FUNCTION();
				overlappedProbesMatched.combine_each(
					[&] ( const boost::dynamic_bitset<> &set ) {
						mergedOverlappedProbesMatched |= set;
					}
				);

				pureProbesMatched.combine_each(
					[&] ( const boost::dynamic_bitset<> &set ) {
						mergedPureProbesMatched |= set;
					}
				);
			}
		}

		const int numQueryProbesMatched = mergedOverlappedProbesMatched.count();
		const int numPureProbesMatched = mergedPureProbesMatched.count();
		MatchInfo matchInfo( id );
		matchInfo.numMatches = numMatches.combine( std::plus<int>() );

		matchInfo.probeMatchPercentage = float( numPureProbesMatched ) / idDataset.size();
		matchInfo.queryMatchPercentage = float( numQueryProbesMatched ) / dataset.size();

		matchInfo.score = matchInfo.probeMatchPercentage * matchInfo.queryMatchPercentage;

		return matchInfo;
	}

	void matchSortedRanges(
		const SortedProbeDataset &overlappedDataset,
		const IntRange &overlappedRange,
		boost::dynamic_bitset<> &overlappedProbesMatched,

		const SortedProbeDataset &pureDataset,
		const IntRange &pureRange,
		boost::dynamic_bitset<> &pureProbesMatched,

		int &numMatches
	) {
		// assert: the range is not empty

		// sort the ranges into two new vectors
		// idea: use a global scratch space to avoid recurring allocations?

		const int beginOverlappedIndex = overlappedRange.first;
		const int endOverlappedIndex = overlappedRange.second;
		int overlappedIndex = beginOverlappedIndex;

		const int beginPureIndex = pureRange.first;
		const int endPureIndex = pureRange.second;
		int pureIndex = beginPureIndex;

		ProbeContext pureContext = pureDataset.getProbeContexts()[ pureIndex ];
		for( ; pureIndex < endPureIndex - 1 ; pureIndex++ ) {
			const ProbeContext nextPureContext = pureDataset.getProbeContexts()[ pureIndex + 1 ];
			int nextBeginOverlappedIndex = overlappedIndex;

			const float minDistance = pureContext.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = pureContext.distance + probeContextTolerance.distanceTolerance;
			const float minNextDistance = nextPureContext.distance - probeContextTolerance.distanceTolerance;

			bool pureProbeMatched = false;

			for( ; overlappedIndex < endOverlappedIndex ; overlappedIndex++ ) {
				const ProbeContext overlappedContext = overlappedDataset.getProbeContexts()[ overlappedIndex ];

				// distance too small?
				if( overlappedContext.distance < minDistance ) {
					// then the next one is too far away as well
					nextBeginOverlappedIndex = overlappedIndex + 1;
					continue;
				}

				// if nextBeginOverlappedIndex can't use this probe, the next overlapped context might be the first one it likes
				if( overlappedContext.distance < minNextDistance ) {
					// set it to the next ref context
					nextBeginOverlappedIndex = overlappedIndex + 1;
				}
				// else:
				//  nextBeginOverlappedIndex points to the first overlapped context the next pure context might match

				// are we past our interval
				if( overlappedContext.distance > maxDistance ) {
					// enough for this probe, do the next
					break;
				}

				if( matchColor( pureContext, overlappedContext ) ) {
					numMatches++;

					overlappedProbesMatched[ overlappedIndex ] = true;
					pureProbeMatched = true;
				}
			}

			if( pureProbeMatched ) {
				pureProbesMatched[ pureIndex ] = true;
			}

			pureContext = nextPureContext;
			overlappedIndex = nextBeginOverlappedIndex;
		}

		// process the last pure probe
		{
			const float minDistance = pureContext.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = pureContext.distance + probeContextTolerance.distanceTolerance;

			bool pureProbeMatched = false;

			for( ; overlappedIndex < endOverlappedIndex ; overlappedIndex++ ) {
				const ProbeContext overlappedContext = overlappedDataset.getProbeContexts()[ overlappedIndex ];

				// distance too small?
				if( overlappedContext.distance < minDistance ) {
					continue;
				}

				// are we past our interval
				if( overlappedContext.distance > maxDistance ) {
					// enough for this probe, we're done
					break;
				}

				if( matchColor( pureContext, overlappedContext ) ) {
					numMatches++;

					overlappedProbesMatched[ overlappedIndex ] = true;
					pureProbeMatched = true;
				}
			}

			if( pureProbeMatched ) {
				pureProbesMatched[ pureIndex ] = true;
			}
		}
	}

#if 0
	void matchRanges(
		const SortedProbeDataset &_overlappedDataset,
		const IntRange &overlappedRange,
		boost::container::vector< bool > &overlappedProbesMatched,

		const SortedProbeDataset &pureDataset,
		const IntRange &pureRange,
		int &numPureProbesMatched,

		int &numMatches
	) {
		// assert: the range is not empty

		// sort the ranges into two new vectors
		// idea: use a global scratch space to avoid recurring allocations?
		const SortedProbeDataset overlappedDataset = _overlappedDataset.subSet( overlappedRange );

		const int overlappedSize = overlappedDataset.size();
		int overlappedIndex = 0;

		const int beginPureIndex = pureRange.first;
		const int endPureIndex = pureRange.second;
		int pureIndex = beginPureIndex;

		ProbeContext pureContext = pureDataset.getProbeContexts()[ pureIndex ];
		for( ; pureIndex < endPureIndex - 1 ; pureIndex++ ) {
			const ProbeContext nextPureContext = pureDataset.getProbeContexts()[ pureIndex + 1 ];
			int nextBeginOverlappedIndex = overlappedIndex;

			const float minDistance = pureContext.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = pureContext.distance + probeContextTolerance.distanceTolerance;
			const float minNextDistance = nextPureContext.distance - probeContextTolerance.distanceTolerance;

			bool pureProbeMatched = false;

			for( ; overlappedIndex < overlappedSize ; overlappedIndex++ ) {
				const ProbeContext overlappedContext = overlappedDataset.getProbeContexts()[ overlappedIndex ];

				// distance too small?
				if( overlappedContext.distance < minDistance ) {
					// then the next one is too far away as well
					nextBeginOverlappedIndex = overlappedIndex + 1;
					continue;
				}

				// if nextBeginOverlappedIndex can't use this probe, the next overlapped context might be the first one it likes
				if( overlappedContext.distance < minNextDistance ) {
					// set it to the next ref context
					nextBeginOverlappedIndex = overlappedIndex + 1;
				}
				// else:
				//  nextBeginOverlappedIndex points to the first overlapped context the next pure context might match

				// are we past our interval
				if( overlappedContext.distance > maxDistance ) {
					// enough for this probe, do the next
					break;
				}

				if( matchColor( pureContext, overlappedContext ) ) {
					numMatches++;

					overlappedProbesMatched[ overlappedIndex + overlappedRange.first ] = true;
					pureProbeMatched = true;
				}
			}

			if( pureProbeMatched ) {
				numPureProbesMatched++;
			}

			pureContext = nextPureContext;
			overlappedIndex = nextBeginOverlappedIndex;
		}

		// process the last pure probe
		{
			const float minDistance = pureContext.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = pureContext.distance + probeContextTolerance.distanceTolerance;

			bool pureProbeMatched = false;

			for( ; overlappedIndex < overlappedSize ; overlappedIndex++ ) {
				const ProbeContext overlappedContext = overlappedDataset.getProbeContexts()[ overlappedIndex ];

				// distance too small?
				if( overlappedContext.distance < minDistance ) {
					continue;
				}

				// are we past our interval
				if( overlappedContext.distance > maxDistance ) {
					// enough for this probe, we're done
					break;
				}

				if( matchColor( pureContext, overlappedContext ) ) {
					numMatches++;

					overlappedProbesMatched[ overlappedIndex + overlappedRange.first ] = true;
					pureProbeMatched = true;
				}
			}

			if( pureProbeMatched ) {
				numPureProbesMatched++;
			}
		}
	}
#endif
#if 0
	void matchRanges(
		const SortedProbeDataset &_overlappedDataset,
		const IntRange &overlappedRange,

		const SortedProbeDataset &pureDataset,
		const IntRange &pureRange,

		int &numMatches
	) {
		// assert: the range is not empty

		// sort the ranges into two new vectors
		// idea: use a global scratch space to avoid recurring allocations?
		const SortedProbeDataset overlappedDataset = _overlappedDataset.subSet( overlappedRange );

		const int overlappedSize = overlappedDataset.size();
		int overlappedIndex = 0;

		const int beginPureIndex = pureRange.first;
		const int endPureIndex = pureRange.second;
		int pureIndex = beginPureIndex;

		ProbeContext overlappedContext = overlappedDataset.getProbeContexts()[ overlappedIndex ];
		for( ; overlappedIndex < overlappedSize - 1 ; overlappedIndex++ ) {
			const ProbeContext nextOverlappedContext = overlappedDataset.getProbeContexts()[ overlappedIndex + 1 ];
			int nextBeginPureIndex = pureIndex;

			const float minDistance = overlappedContext.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = overlappedContext.distance + probeContextTolerance.distanceTolerance;
			const float minNextDistance = nextOverlappedContext.distance - probeContextTolerance.distanceTolerance;

			// assert: minDistance <= minNextDistance
			for( ; pureIndex < endPureIndex ; pureIndex++ ) {
				const ProbeContext &pureContext = pureDataset.getProbeContexts()[ pureIndex ];

				// distance too small?
				if( pureContext.distance < minDistance ) {
					// then the next one is too far away as well
					nextBeginPureIndex = pureIndex + 1;
					continue;
				}

				// if nextQueryContext can't use this probe, the next ref context might be the first one it likes
				if( pureContext.distance < minNextDistance ) {
					// set it to the next ref context
					nextBeginPureIndex = pureIndex + 1;
				}
				// else:
				//  nextStartRefIndex points to the first ref content the next query context might match

				// are we past our interval
				if( pureContext.distance > maxDistance ) {
					// enough for this probe, do the next
					break;
				}

				if( matchColor( pureContext, overlappedContext ) ) {
					numMatches++;
				}
			}

			overlappedContext = nextOverlappedContext;
			pureIndex = nextBeginPureIndex;
		}

		// process the last element
		{
			const float minDistance = overlappedContext.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = overlappedContext.distance + probeContextTolerance.distanceTolerance;

			for( ; pureIndex < endPureIndex ; pureIndex++ ) {
				const ProbeContext &pureContext = pureDataset.getProbeContexts()[ pureIndex ];

				// distance too small?
				if( pureContext.distance < minDistance ) {
					continue;
				}
				if( pureContext.distance > maxDistance ) {
					// done
					break;
				}

				if( matchColor( pureContext, overlappedContext ) ) {
					numMatches++;
				}
			}
		}
	}
#endif
	__forceinline__ bool matchColor( const ProbeContext &refContext, const ProbeContext &queryContext ) {
		Eigen::Vector3i colorDistance(
			refContext.Lab.x - queryContext.Lab.x,
			refContext.Lab.y - queryContext.Lab.y,
			refContext.Lab.z - queryContext.Lab.z
			);
		// TODO: cache the last value? [10/1/2012 kirschan2]
		if( colorDistance.squaredNorm() > probeContextTolerance.colorLabTolerance * probeContextTolerance.colorLabTolerance ) {
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
	const ProbeDatabase &database;

	IndexedProbeDataset dataset;

	ProbeContextTolerance probeContextTolerance;

	MatchInfos matchInfos;
};

struct ProbeDatabase::FullQuery {
	typedef std::shared_ptr<FullQuery> Ptr;

	struct MatchInfo {
		struct ProbeMatch {
			optix::float3 refDirection;
			optix::float3 queryDirection;
			optix::float3 refPosition;

			ProbeMatch() {}

			ProbeMatch( const optix::float3 &refDirection, const optix::float3 &queryDirection, const optix::float3 &refPosition )
				: refDirection( refDirection )
				, refPosition( refPosition )
				, queryDirection( queryDirection )
			{}
		};

		int id;

		//std::vector< ProbeMatch > matches;
		Concurrency::concurrent_vector< ProbeMatch > matches;

		MatchInfo( int id = -1 ) : id( id ) {}

		MatchInfo( MatchInfo &&other )
			: id( other.id )
			, matches( matches )
		{
		}

		MatchInfo & operator = ( MatchInfo &&other ) {
			id = other.id;
			matches.swap( other.matches );

			other.id = -1;
			other.matches.clear();

			return *this;
		}

	private:
		MatchInfo( const MatchInfo & );
		MatchInfo & operator = ( const MatchInfo &other );
	};
	typedef std::vector<MatchInfo> MatchInfos;

	FullQuery( const ProbeDatabase &database ) : database( database ) {}

	void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
		probeContextTolerance = pct;
	}

	void setQueryDataset( SortedProbeDataset &&dataset ) {
		this->dataset = std::move( dataset );
	}

	MatchInfos execute() {
		MatchInfos matchInfos;
		matchInfos.resize( database.idDatasets.size() );

		using namespace Concurrency;

		AUTO_TIMER_MEASURE() {
			int logScope = Log::getScope();

			parallel_for< int >(
				0,
				database.idDatasets.size(),
				[&] ( int id ) {
					Log::initThreadScope( logScope, 0 );

					matchInfos[ id ] = matchAgainst( id );
				}
			);
		}

		boost::remove_erase_if( matchInfos, [] ( const MatchInfo &matchInfo ) { return matchInfo.matches.empty(); });
		return matchInfos;
	}

protected:
	typedef IndexedProbeDataset::IntRange IntRange;
	typedef std::pair< IntRange, IntRange > OverlappedRange;

	MatchInfo matchAgainst( int id ) {
		// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
		const IndexedProbeDataset &idDataset = database.idDatasets[id].mergedDataset;

		if( idDataset.size() == 0 ) {
			return MatchInfo( id );
		}

		AUTO_TIMER_FOR_FUNCTION( boost::format( "id = %i, %i ref probes (%i query probes)" ) % id % idDataset.size() % dataset.size() );

		// idea:
		//	use a binary search approach to generate only needed subranges

		// we can compare the different occlusion ranges against each other, after including the tolerance

		// TODO: is it better to make both ranges about equally big or not?
		// its better they are equal

		// assuming that the query set is smaller, we enlarge it, to have less items to sort than vice-versa
		// we could determine this at runtime...
		// if( idDatasets.size() > dataset.size() ) {...} else {...}
		const int occlusionTolerance = int( OptixProgramInterface::numProbeSamples * probeContextTolerance.occusionTolerance + 0.5 );

		// TODO: use a stack allocated array here? [9/27/2012 kirschan2]

		std::vector< OverlappedRange > overlappedRanges;
		overlappedRanges.reserve( OptixProgramInterface::numProbeSamples );
		for( int occulsionLevel = 0 ; occulsionLevel <= OptixProgramInterface::numProbeSamples ; occulsionLevel++ ) {
			const IndexedProbeDataset::IntRange queryRange = dataset.getOcclusionRange( occulsionLevel );

			if( queryRange.first == queryRange.second ) {
				continue;
			}

			const int leftToleranceLevel = std::max( 0, occulsionLevel - occlusionTolerance );
			const int rightToleranceLevel = std::min( occulsionLevel + occlusionTolerance, OptixProgramInterface::numProbeSamples );
			for( int idToleranceLevel = leftToleranceLevel ; idToleranceLevel <= rightToleranceLevel ; idToleranceLevel++ ) {
				const IndexedProbeDataset::IntRange idRange = idDataset.getOcclusionRange( idToleranceLevel );

				// is one of the ranges empty? if so, we don't need to check it at all
				if( idRange.first == idRange.second ) {
					continue;
				}

				// store the range for later
				overlappedRanges.push_back( std::make_pair( queryRange, idRange ) );
			}
		}

		MatchInfo matchInfo( id );
		using namespace Concurrency;
		matchInfo.matches.reserve( std::max( dataset.size(), idDataset.size() ) );

		AUTO_TIMER_BLOCK( "matching" ) {
			parallel_for_each( overlappedRanges.begin(), overlappedRanges.end(),
				[&] ( const OverlappedRange &rangePair ) {
					matchSortedRanges(
						matchInfo.matches,

						dataset.data,
						rangePair.first,

						idDataset.data,
						rangePair.second
					);
				}
			);
		}

		return matchInfo;
	}

	inline static void pushMatch( Concurrency::concurrent_vector< MatchInfo::ProbeMatch > &matches, const Probe &refProbe, const Probe &queryProbe ) {
		auto match = matches.grow_by( 1 );
		match->refDirection = refProbe.direction;
		match->refPosition = refProbe.position;
		match->queryDirection = queryProbe.direction;
	}

	void matchSortedRanges(
		Concurrency::concurrent_vector< MatchInfo::ProbeMatch > &matches,

		const SortedProbeDataset &queryDataset,
		const IntRange &queryRange,

		const SortedProbeDataset &refDataset,
		const IntRange &refRange
	) {
		// assert: the range is not empty

		// sort the ranges into two new vectors
		// idea: use a global scratch space to avoid recurring allocations?

		const int beginQueryIndex = queryRange.first;
		const int endQueryIndex = queryRange.second;
		int queryIndex = beginQueryIndex;

		const int beginrefIndex = refRange.first;
		const int endrefIndex = refRange.second;
		int refIndex = beginrefIndex;

		ProbeContext refContext = refDataset.getProbeContexts()[ refIndex ];
		for( ; refIndex < endrefIndex - 1 ; refIndex++ ) {
			const ProbeContext nextrefContext = refDataset.getProbeContexts()[ refIndex + 1 ];
			int nextBeginQueryIndex = queryIndex;

			const float minDistance = refContext.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = refContext.distance + probeContextTolerance.distanceTolerance;
			const float minNextDistance = nextrefContext.distance - probeContextTolerance.distanceTolerance;

			const auto refProbe = refDataset.getProbes()[ refIndex ];

			for( ; queryIndex < endQueryIndex ; queryIndex++ ) {
				const ProbeContext queryContext = queryDataset.getProbeContexts()[ queryIndex ];

				// distance too small?
				if( queryContext.distance < minDistance ) {
					// then the next one is too far away as well
					nextBeginQueryIndex = queryIndex + 1;
					continue;
				}

				// if nextBeginQueryIndex can't use this probe, the next query context might be the first one it likes
				if( queryContext.distance < minNextDistance ) {
					// set it to the next ref context
					nextBeginQueryIndex = queryIndex + 1;
				}
				// else:
				//  nextBeginQueryIndex points to the first query context the next ref context might match

				// are we past our interval
				if( queryContext.distance > maxDistance ) {
					// enough for this probe, do the next
					break;
				}

				if( matchColor( refContext, queryContext ) ) {
					pushMatch( matches, refProbe, queryDataset.getProbes()[ queryIndex ] );
				}
			}

			refContext = nextrefContext;
			queryIndex = nextBeginQueryIndex;
		}

		// process the last ref probe
		{
			const float minDistance = refContext.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = refContext.distance + probeContextTolerance.distanceTolerance;

			const auto refProbe = refDataset.getProbes()[ refIndex ];

			for( ; queryIndex < endQueryIndex ; queryIndex++ ) {
				const ProbeContext queryContext = queryDataset.getProbeContexts()[ queryIndex ];

				// distance too small?
				if( queryContext.distance < minDistance ) {
					continue;
				}

				// are we past our interval
				if( queryContext.distance > maxDistance ) {
					// enough for this probe, we're done
					break;
				}

				if( matchColor( refContext, queryContext ) ) {
					pushMatch( matches, refProbe, queryDataset.getProbes()[ queryIndex ] );
				}
			}
		}
	}

	__forceinline__ bool matchColor( const ProbeContext &refContext, const ProbeContext &queryContext ) {
		Eigen::Vector3i colorDistance(
			refContext.Lab.x - queryContext.Lab.x,
			refContext.Lab.y - queryContext.Lab.y,
			refContext.Lab.z - queryContext.Lab.z
			);
		// TODO: cache the last value? [10/1/2012 kirschan2]
		if( colorDistance.squaredNorm() > probeContextTolerance.colorLabTolerance * probeContextTolerance.colorLabTolerance ) {
			return false;
		}
		return true;
	}

protected:
	const ProbeDatabase &database;

	IndexedProbeDataset dataset;

	ProbeContextTolerance probeContextTolerance;

private:
	FullQuery( const FullQuery & );
	FullQuery & operator = ( const FullQuery & );
};
