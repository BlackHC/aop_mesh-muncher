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
#include "boost/range/algorithm/transform.hpp"
#include "boost/range/numeric.hpp"

#include "optixEigenInterop.h"

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

#if 0
struct RawProbeDataset {
	typedef OptixProgramInterface::Probe Probe;
	typedef OptixProgramInterface::ProbeContext ProbeContext;

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
#endif

typedef std::vector< OptixProgramInterface::ProbeContext > RawProbeDataset;

// no further preprocessed information
// invariant: sorted
struct SortedProbeDataset {
	typedef OptixProgramInterface::Probe Probe;

	struct ProbeContext : OptixProgramInterface::ProbeContext {
		// sometime's we're lucky and we can compress probes
		int weight;
		int probeIndex;

		void setFrom( int probeIndex, const OptixProgramInterface::ProbeContext &context ) {
			this->probeIndex = probeIndex;
			weight = 1;

			*static_cast<  OptixProgramInterface::ProbeContext* >( this ) = context;			
		}

		static __forceinline__ bool lexicographicalLess( const ProbeContext &a, const ProbeContext &b ) {
			return
				boost::make_tuple( a.hitCounter, a.distance, a.Lab.x, a.Lab.y, a.Lab.z )
				<
				boost::make_tuple( b.hitCounter, b.distance, b.Lab.x, a.Lab.y, a.Lab.z )
			;
		}

		static __forceinline__ bool lexicographicalLess_startWithDistance( const ProbeContext &a, const ProbeContext &b ) {
			return
				boost::make_tuple( a.distance, a.Lab.x, a.Lab.y, a.Lab.z )
				<
				boost::make_tuple( b.distance, b.Lab.x, a.Lab.y, a.Lab.z )
			;
		}
	};

	SortedProbeDataset() {}

	SortedProbeDataset( const RawProbeDataset &other )
	{
		data.resize( other.size() );

		// set the context
		for( int probeIndex = 0 ; probeIndex < data.size() ; probeIndex++ ) {
			data[ probeIndex ].setFrom( probeIndex, other[ probeIndex ] );
		}

		sort();
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
		cloned.data = data;
		return cloned;
	}

	int size() const {
		return data.size();
	}

	static SortedProbeDataset merge( const SortedProbeDataset &first, const SortedProbeDataset &second );
	static SortedProbeDataset mergeMultiple( const std::vector< const SortedProbeDataset* > &datasets);

	SortedProbeDataset subSet( const std::pair< int, int > &range ) const;

	const std::vector< ProbeContext > &getProbeContexts() const {
		return data;
	}

	void sort() {
		AUTO_TIMER_FOR_FUNCTION();

		boost::sort( data, ProbeContext::lexicographicalLess );
	}

private:
	std::vector< ProbeContext > &probeContexts() {
		return data;
	}

	std::vector< ProbeContext > data;

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
		colorLabTolerance = 5;
		distanceTolerance = 0.25;
	}
};

struct ProbeDatabase {
	typedef int Id;
	typedef std::vector<Id> Ids;

	struct IdDatasets {
		typedef IndexedProbeDataset::Probe Probe;
		typedef IndexedProbeDataset::ProbeContext ProbeContext;

		std::vector<SortedProbeDataset> insertQueue;
		IndexedProbeDataset mergedDataset;

		typedef std::vector< Probe > Probes;
		Probes probes;

		int numInstances;

		void addToQueue( const Probes &datasetProbes, SortedProbeDataset &&dataset ) {
			if( probes.size() != datasetProbes.size() ) {
				if( !probes.empty() ) {
					logError( 
						boost::format( 
						"expected %i probes, but got %i probes!\ndumping %i merged + %i queued instances = %i sampled probes and reseting this dataset!"
						)
						% probes.size()
						% datasetProbes.size()
						% numInstances
						% insertQueue.size()
						% ((numInstances + insertQueue.size()) * probes.size())
					);
					// reset the dataset
					insertQueue.clear();
					mergedDataset = IndexedProbeDataset();
				}
					
				numInstances = 0;
				probes = datasetProbes;
			}

			insertQueue.emplace_back( std::move( dataset ) );
		}

		IdDatasets() 
			: numInstances()
		{}

		IdDatasets( IdDatasets &&other ) 
			: insertQueue( std::move( other.insertQueue ) )
			, mergedDataset( std::move( other.mergedDataset ) )
			, probes( std::move( other.probes ) )
			, numInstances( numInstances )
		{}

		IdDatasets & operator = ( IdDatasets &&other ) {
			insertQueue = std::move( other.insertQueue );
			mergedDataset = std::move( other.mergedDataset );
			probes = std::move( other.probes );
			numInstances = other.numInstances;

			other.numInstances = 0;

			return *this;
		}

		void processQueue() {
			if( insertQueue.empty() ) {
				return;
			}

			// update numInstances
			numInstances += insertQueue.size();

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
	struct WeightedQuery;
	struct FullQuery;

	void reserveIds( Id maxId ) {
		idDatasets.resize( maxId + 1 );
	}

	void addDataset( Id id, const IdDatasets::Probes &datasetProbes, SortedProbeDataset &&dataset ) {
		idDatasets[ id ].addToQueue( datasetProbes, std::move( dataset ) );
	}

	void integrateDatasets() {
		for( auto idDataset = idDatasets.begin() ; idDataset != idDatasets.end() ; ++idDataset ) {
			idDataset->processQueue();
		}
	}

	size_t getNumIdDatasets() const {
		return idDatasets.size();
	}
	
	bool isEmpty( int modelIndex ) const {
		return idDatasets[ modelIndex ].mergedDataset.size() == 0;
	}

	// see candidateFinderCache.cpp
	bool loadCache( const char *filename );
	void storeCache( const char *filename );

public:
	void reset() {
		int numIds = idDatasets.size();
		idDatasets.clear();
		idDatasets.resize( numIds );
	}

private:
	std::vector<IdDatasets> idDatasets;
};

#include "probeDatabaseQueries.h"