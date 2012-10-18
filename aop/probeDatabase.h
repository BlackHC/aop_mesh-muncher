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

#include <serializer_fwd.h>
#include <algorithm>

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
struct InstanceProbeDataset;
struct IndexedProbeDataset;
struct IdDatasets;

SERIALIZER_FWD_EXTERN_DECL( InstanceProbeDataset )
SERIALIZER_FWD_EXTERN_DECL( IndexedProbeDataset )
SERIALIZER_FWD_EXTERN_DECL( IdDatasets )

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

struct ProbeContextTolerance {
	// tolerance means: |x-y| < tolerance, then match

	float occusionTolerance;
	float colorLabTolerance;
	// this should be the resolution of the probes
	float distanceTolerance;

	ProbeContextTolerance() {
		setDefault();
	}

	void setDefault() {
		occusionTolerance = 0.25;
		colorLabTolerance = 5;
		distanceTolerance = 0.25;
	}
};

struct ProbeContextToleranceV2 {
	// tolerance means: |x-y| < tolerance, then match

	int occlusion_integerTolerance;
	float colorLab_squaredTolerance;
	// this should be the resolution of the probes
	float distance_tolerance;

	ProbeContextToleranceV2() {
		setDefault();
	}

	ProbeContextToleranceV2(
		int occusion_integerTolerance,
		float colorLab_squaredTolerance,
		float distance_tolerance
	)
		: occlusion_integerTolerance( occlusion_integerTolerance )
		, colorLab_squaredTolerance( colorLab_squaredTolerance )
		, distance_tolerance( distance_tolerance )
	{
	}

	void setDefault() {
		occlusion_integerTolerance = 0.25 * OptixProgramInterface::numProbeSamples;
		colorLab_squaredTolerance = 25;
		distance_tolerance = 0.25;
	}
};

typedef std::vector< OptixProgramInterface::ProbeContext > RawProbeDataset;

// no further preprocessed information
// invariant: sorted
struct InstanceProbeDataset {
	typedef OptixProgramInterface::Probe Probe;

	struct ProbeContext : OptixProgramInterface::ProbeContext {
		// sometime's we're lucky and we can compress probes
		int weight;
		int probeIndex;

		void assign( int probeIndex, const OptixProgramInterface::ProbeContext &context ) {
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

		static __forceinline__ bool less_byId( const ProbeContext &a, const ProbeContext &b ) {
			return a.probeIndex < b.probeIndex;
		}

		static __forceinline__ bool lexicographicalLess_startWithDistance( const ProbeContext &a, const ProbeContext &b ) {
			return
				boost::make_tuple( a.distance, a.Lab.x, a.Lab.y, a.Lab.z )
				<
				boost::make_tuple( b.distance, b.Lab.x, a.Lab.y, a.Lab.z )
			;
		}

		static __forceinline__ bool matchColor( const ProbeContext &a, const ProbeContext &b, const float squaredTolerance ) {
			Eigen::Vector3i colorDistance(
				a.Lab.x - b.Lab.x,
				a.Lab.y - b.Lab.y,
				a.Lab.z - b.Lab.z
				);
			// TODO: cache the squared? [10/1/2012 kirschan2]
			if( colorDistance.squaredNorm() > squaredTolerance ) {
				return false;
			}
			return true;
		}

		static __forceinline__ bool matchDistance( const ProbeContext &a, const ProbeContext &b, const float tolerance ) {
			return fabs( a.distance - b.distance ) <= tolerance;
		}

		static __forceinline__ bool matchOcclusion( const ProbeContext &a, const ProbeContext &b, const int integerTolerance) {
			int absDelta = a.hitCounter - b.hitCounter;
			if( absDelta < 0 ) {
				absDelta = -absDelta;
			}
			return absDelta <= integerTolerance;
		}

		static __forceinline__ bool matchOcclusionDistanceColor(
			const ProbeContext &a,
			const ProbeContext &b,
			const int occlusion_integerTolerance,
			const float distance_tolerance,
			const float color_squaredTolerance
		) {
			return
					matchOcclusion( a, b, occlusion_integerTolerance )
				&&
					matchDistance( a, b, distance_tolerance )
				&&
					matchColor( a, b, color_squaredTolerance )
			;
		}
	};

	InstanceProbeDataset() {}

	InstanceProbeDataset( const RawProbeDataset &other )
	{
		data.resize( other.size() );

		// set the context
		for( int probeIndex = 0 ; probeIndex < data.size() ; probeIndex++ ) {
			data[ probeIndex ].assign( probeIndex, other[ probeIndex ] );
		}

		//sort();
	}

	InstanceProbeDataset( InstanceProbeDataset &&other ) :
		data( std::move( other.data ) )
	{
	}

	InstanceProbeDataset & operator = ( InstanceProbeDataset &&other ) {
		data = std::move( other.data );

		return *this;
	}

	InstanceProbeDataset clone() const {
		InstanceProbeDataset cloned;
		cloned.data = data;
		return cloned;
	}

	int size() const {
		return data.size();
	}

	static InstanceProbeDataset merge( const InstanceProbeDataset &first, const InstanceProbeDataset &second );
	static InstanceProbeDataset mergeMultiple( const std::vector< const InstanceProbeDataset* > &datasets);

	InstanceProbeDataset subSet( const std::pair< int, int > &range ) const;

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

	SERIALIZER_FWD_FRIEND_EXTERN( InstanceProbeDataset )

private:
	// better error messages than with boost::noncopyable
	InstanceProbeDataset( const InstanceProbeDataset &other );
	InstanceProbeDataset & operator = ( const InstanceProbeDataset &other );
};

namespace CompressedDataset {
	typedef InstanceProbeDataset::ProbeContext ProbeContext;
	typedef std::vector< ProbeContext > ProbeContexts;

	static void compress( int numInstances, int numProbes, ProbeContexts &data, const ProbeContextToleranceV2 &pct ) {
		AUTO_TIMER_FUNCTION();

		AUTO_TIMER_BLOCK( "presorting" ) {
			// TODO: we know how the probe contests are interleaved, so we can just permute them
			boost::sort( data, ProbeContext::less_byId );
		}

		AUTO_TIMER_BLOCK( "compressing" ) {
			ProbeContexts compressedData;
			compressedData.reserve( data.size() );

			for( int probeIndex = 0 ; probeIndex < numProbes ; ++probeIndex ) {
				const auto probeContext_end = data.begin() + (probeIndex + 1) * numInstances;
				auto probeContext_mergeBegin = data.begin() + probeIndex * numInstances;
				while( probeContext_mergeBegin != probeContext_end ) {
					auto probeContext_mergeEnd = std::partition( probeContext_mergeBegin, probeContext_end,
						[probeContext_mergeBegin, pct] ( const ProbeContext &context ) {
							return ProbeContext::matchOcclusionDistanceColor( 
								*probeContext_mergeBegin,
								context,
								pct.occlusion_integerTolerance,
								pct.distance_tolerance,
								pct.colorLab_squaredTolerance
							);
						}
					);

					auto probeContext_median = probeContext_mergeBegin + (probeContext_mergeEnd - probeContext_mergeBegin) / 2;
					// TODO: we can do this in place without a second vector [10/17/2012 kirschan2]
					compressedData.push_back( *probeContext_median );
					compressedData.back().weight = probeContext_mergeEnd - probeContext_mergeBegin;

					probeContext_mergeBegin = probeContext_mergeEnd;
				}
			}

			log(
				boost::format( "compression ratio: %f, %i probes into %i probes" )
				% (float( compressedData.size() ) / data.size() )
				% data.size()
				% compressedData.size()
			);

			data = std::move( compressedData );
		}
	}
}

#if 0
struct CompressedDataset {
	typedef InstanceProbeDataset::ProbeContext ProbeContext;
	typedef std::vector< ProbeContext > ProbeContexts;

	CompressedDataset( ProbeContexts &&mergedProbeContexts )
		: data( std::move( probeContexts() ) )
	{
		compress( ProbeContextToleranceV2( 1, 1, 0.25 * 0.95 ) );
	}

	const std::vector< ProbeContext > &getProbeContexts() const {
		return data;
	}

private:
	

private:
	std::vector< ProbeContext > &probeContexts() {
		return data;
	}

	ProbeContexts data;

	SERIALIZER_FWD_FRIEND_EXTERN( CompressedDataset )
};
#endif

// this dataset creates auxiliary structures automatically
// invariant: sorted and hitCounterLowerBounds is correctly set
struct IndexedProbeDataset {
	typedef InstanceProbeDataset::Probe Probe;
	typedef InstanceProbeDataset::ProbeContext ProbeContext;
	typedef std::vector< ProbeContext > ProbeContexts;

	ProbeContexts data;
	std::vector<int> hitCounterLowerBounds;

	const ProbeContexts &getProbeContexts() const {
		return data;
	}
	
	IndexedProbeDataset() {}

	IndexedProbeDataset( ProbeContexts &&other ) :
		data( std::move( other ) ),
		hitCounterLowerBounds()
	{
		sort();
		setHitCounterLowerBounds();
	}

	IndexedProbeDataset( const InstanceProbeDataset &other ) :
		data( other.getProbeContexts() ),
		hitCounterLowerBounds()
	{
		sort();
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
		cloned.data = data;
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
	void sort() {
		AUTO_TIMER_FUNCTION();

		boost::sort( data, ProbeContext::lexicographicalLess );
	}

	void setHitCounterLowerBounds();

	SERIALIZER_FWD_FRIEND_EXTERN( IndexedProbeDataset )
private:
	// better error messages than with boost::noncopyable
	IndexedProbeDataset( const IndexedProbeDataset &other );
	IndexedProbeDataset & operator = ( const IndexedProbeDataset &other );
};

struct IdDatasets {
	typedef IndexedProbeDataset::Probe Probe;
	typedef IndexedProbeDataset::ProbeContext ProbeContext;

	typedef std::vector<InstanceProbeDataset> Instances;
	typedef std::vector< ProbeContext > ProbeContexts;
	typedef std::vector< Probe > Probes;

	void addInstances( const Probes &datasetProbes, InstanceProbeDataset &&dataset ) {
		if( probes.size() != datasetProbes.size() ) {
			if( !probes.empty() ) {
				logError(
					boost::format(
					"expected %i probes, but got %i probes!\ndumping %i instances (%i probes) and reseting this dataset!"
					)
					% probes.size()
					% datasetProbes.size()
					% instances.size()
					% mergedInstances.size()
				);
				// reset the dataset
				instances.clear();
				mergedInstances = IndexedProbeDataset();
			}

			probes = datasetProbes;
		}

		instances.emplace_back( std::move( dataset ) );
	}

	void clear() {
		instances.clear();
		mergedInstances = IndexedProbeDataset();
		probes.clear();
	}

	IdDatasets()
	{}

	IdDatasets( IdDatasets &&other )
		: instances( std::move( other.instances ) )
		, mergedInstances( std::move( other.mergedInstances ) )
		, probes( std::move( other.probes ) )
	{}

	IdDatasets & operator = ( IdDatasets &&other ) {
		instances = std::move( other.instances );
		mergedInstances = std::move( other.mergedInstances );
		probes = std::move( other.probes );

		return *this;
	}

	void mergeInstances() {
		if( instances.empty() ) {
			return;
		}
		else if( instances.size() == 1 ) {
			mergedInstances = IndexedProbeDataset( instances[0] );
		}
		else {
			const int totalSize =  boost::accumulate( 
				instances,
				0,
				[] ( int totalLength, const InstanceProbeDataset &dataset ) {
					return totalLength + dataset.size();
				}
			);

			ProbeContexts probeContexts;
			probeContexts.reserve( totalSize );

			AUTO_TIMER_BLOCK( "push back all instances" ) {
				for( auto instance = instances.begin() ; instance != instances.end() ; ++instance ) {
					boost::push_back( probeContexts, instance->getProbeContexts() );
				}
			}

			// TODO: magic constants!!! [10/17/2012 kirschan2]
			CompressedDataset::compress( instances.size(), probes.size(), probeContexts, ProbeContextToleranceV2( 0.124 * OptixProgramInterface::numProbeSamples, 1, 0.25 * 0.95 ) );
			
			mergedInstances = IndexedProbeDataset( std::move( probeContexts ) );
		}
	}

	bool isEmpty() const {
		return mergedInstances.size() == 0;
	}

	const Instances & getInstances() const {
		return instances;
	}

	const IndexedProbeDataset & getMergedInstances() const {
		return mergedInstances;
	}

	const Probes &getProbes() const {
		return probes;
	}

private:
	Instances instances;
	IndexedProbeDataset mergedInstances;

	Probes probes;

	SERIALIZER_FWD_FRIEND_EXTERN( IdDatasets )

private:
	// better error messages than with boost::noncopyable
	IdDatasets( const IdDatasets &other );
	IdDatasets & operator = ( const IdDatasets &other );
};

struct ProbeDatabase {
	typedef int Id;
	typedef std::vector<Id> Ids;

	typedef IdDatasets::Probe Probe;
	typedef IdDatasets::ProbeContext ProbeContext;
	typedef std::vector< ProbeContext > ProbeContexts;

	// TODO: fix the naming [10/15/2012 kirschan2]
	typedef std::vector<IdDatasets> IdDatasetsVector;

	struct Query;
	struct WeightedQuery;
	struct FullQuery;

	void reserveIds( Id maxId ) {
		idDatasets.resize( maxId + 1 );
	}

	void addDataset( Id id, const IdDatasets::Probes &datasetProbes, InstanceProbeDataset &&dataset ) {
		idDatasets[ id ].addInstances( datasetProbes, std::move( dataset ) );
	}

	void integrateDatasets() {
		for( auto idDataset = idDatasets.begin() ; idDataset != idDatasets.end() ; ++idDataset ) {
			idDataset->mergeInstances();
		}
	}

	size_t getNumIdDatasets() const {
		return idDatasets.size();
	}

	bool isEmpty( int modelIndex ) const {
		return idDatasets[ modelIndex ].isEmpty();
	}

	// see candidateFinderCache.cpp
	bool loadCache( const char *filename );
	void storeCache( const char *filename );

	void clear() {
		int numIds = idDatasets.size();
		idDatasets.clear();
		idDatasets.resize( numIds );
	}

	const IdDatasetsVector & getIdDatasets() const {
		return idDatasets;
	}

private:
	IdDatasetsVector idDatasets;
};

#include "probeDatabaseQueries.h"