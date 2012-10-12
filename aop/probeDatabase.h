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

// TODO: add a serializer forward header with macros to declare these functions [10/3/2012 kirschan2]
struct SortedProbeDataset;
struct ProbeDataset;

namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, SortedProbeDataset &value );
	template< typename Writer >
	void write( Writer &writer, const SortedProbeDataset &value );

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
struct ProbeDataset {
	SortedProbeDataset data;

	const std::vector< Probe > &getProbes() const {
		return data.getProbes();
	}

	const std::vector< ProbeContext > &getProbeContexts() const {
		return data.getProbeContexts();
	}

	std::vector<int> hitCounterLowerBounds;

	ProbeDataset() {}

	ProbeDataset( SortedProbeDataset &&other ) :
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
		colorTolerance = 0.10;
		distanceTolerance = 0.25;
	}
};

struct ProbeDatabase {
	typedef int Id;
	typedef std::vector<Id> Ids;

	struct MatchInfo {
		int id;
		int numMatches;
		float score;

		float queryMatchPercentage;
		float probeMatchPercentage;

		MatchInfo( int id = -1 ) : id( id ), numMatches(), score(), queryMatchPercentage(), probeMatchPercentage() {}
	};

	struct Query {
		typedef std::shared_ptr<Query> Ptr;

		typedef ProbeDatabase::MatchInfo MatchInfo;
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
			for( int id = 0 ; id < database.idDatasets.size() ; id++ ) {
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
		typedef std::pair< IntRange, IntRange > OverlappedRange;

#if 0
		struct ProbeMatch {
			optix::float3 position;
			float radius;
		};
#endif

		MatchInfo matchAgainst( int id ) {
			// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
			const ProbeDataset &idDataset = database.idDatasets[id].mergedDataset;

			if( idDataset.size() == 0 ) {
				return MatchInfo( id );
			}

			AUTO_TIMER_FOR_FUNCTION( "id = " + boost::lexical_cast<std::string>( id ) );

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
				const ProbeDataset::IntRange queryRange = dataset.getOcclusionRange( occulsionLevel );

				if( queryRange.first == queryRange.second ) {
					continue;
				}

				const int leftToleranceLevel = std::max( 0, occulsionLevel - occlusionTolerance );
				const int rightToleranceLevel = std::min( occulsionLevel + occlusionTolerance, OptixProgramInterface::numProbeSamples );
				for( int idToleranceLevel = leftToleranceLevel ; idToleranceLevel <= rightToleranceLevel ; idToleranceLevel++ ) {
					const ProbeDataset::IntRange idRange = idDataset.getOcclusionRange( idToleranceLevel );

					// is one of the ranges empty? if so, we don't need to check it at all
					if( idRange.first == idRange.second ) {
						continue;
					}

					// store the range for later
					overlappedRanges.push_back( std::make_pair( queryRange, idRange ) );
				}
			}

			boost::container::vector< bool > overlappedProbesMatched( dataset.size() );
			boost::container::vector< bool > pureProbesMatched( idDataset.size() );

			int numMatches = 0;
			for( auto rangePair = overlappedRanges.begin() ; rangePair != overlappedRanges.end() ; ++rangePair ) {
				matchSortedRanges( 
					dataset.data,
					rangePair->first,
					overlappedProbesMatched,

					idDataset.data,
					rangePair->second,
					pureProbesMatched,

					numMatches
				);
			}

			const int numQueryProbesMatched = boost::count( overlappedProbesMatched, true );
			const int numPureProbesMatched = boost::count( pureProbesMatched, true );
			MatchInfo matchInfo( id );
			matchInfo.numMatches = numMatches;

			matchInfo.probeMatchPercentage = float( numPureProbesMatched ) / idDataset.size();
			matchInfo.queryMatchPercentage = float( numQueryProbesMatched ) / dataset.size();

			matchInfo.score = matchInfo.probeMatchPercentage * matchInfo.queryMatchPercentage;

			return matchInfo;
		}


		void matchSortedRanges(
			const SortedProbeDataset &overlappedDataset,
			const IntRange &overlappedRange,
			boost::container::vector< bool > &overlappedProbesMatched,

			const SortedProbeDataset &pureDataset,
			const IntRange &pureRange,
			boost::container::vector< bool > &pureProbesMatched,

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
		const ProbeDatabase &database;

		ProbeDataset dataset;

		ProbeContextTolerance probeContextTolerance;

		MatchInfos matchInfos;
	};

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
		std::vector<SortedProbeDataset> insertQueue;
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

	void reset() {
		int numIds = idDatasets.size();
		idDatasets.clear();
		idDatasets.resize( numIds );
	}

private:
	std::vector<IdDatasets> idDatasets;
};
