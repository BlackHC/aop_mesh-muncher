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
#include <unordered_map>

#include "mathUtility.h"

#include "probeGenerator.h"

#include <autoTimer.h>

#include <boost/lexical_cast.hpp>
#include <sort_permute_iter.h>
#include "boost/range/algorithm/sort.hpp"
#include <boost/iterator/counting_iterator.hpp>

#include "queryResult.h"

#include "flatmultimap.h"

namespace ProbeContext {

struct InstanceProbeDataset;
struct IndexedProbeSamples;
struct SampledModel;
struct ProbeDatabase;

}

SERIALIZER_FWD_EXTERN_DECL( ProbeContext::InstanceProbeDataset )
SERIALIZER_FWD_EXTERN_DECL( ProbeContext::IndexedProbeSamples )
SERIALIZER_FWD_EXTERN_DECL( ProbeContext::SampledModel )
SERIALIZER_FWD_EXTERN_DECL( ProbeContext::ProbeDatabase )

namespace ProbeContext {

struct ModelIndexMapper {
	static const int INVALID_INDEX = -1;

	std::vector<int> localToScene;
	std::vector<int> sceneToLocal;

	std::vector< std::string > sceneModelNames;
	std::unordered_map< std::string, int > modelNamesMap;

	ModelIndexMapper() : modelNamesMap() {}

	void registerSceneModels( const std::vector< std::string > &sceneModelNames ) {
		this->sceneModelNames = sceneModelNames;

		modelNamesMap.clear();
		const int numModels = (int) sceneModelNames.size();
		for( int sceneModelIndex = 0 ; sceneModelIndex < numModels ; sceneModelIndex++ ) {
			modelNamesMap[ sceneModelNames[ sceneModelIndex ] ] = sceneModelIndex;
		}
		resetLocalMaps();
	}

	void resetLocalMaps() {
		localToScene.clear();
		sceneToLocal.assign( sceneModelNames.size(), INVALID_INDEX );
	}

	void registerLocalModels( const std::vector< std::string > &localModelNames ) {
		resetLocalMaps();

		for( auto localModelName = localModelNames.begin() ; localModelName != localModelNames.end() ; ++localModelName ) {
			registerLocalModel( *localModelName );
		}
	}

	void registerLocalModel( const std::string &localModelName ) {
		auto found = modelNamesMap.find( localModelName );
		if( found != modelNamesMap.end() ) {
			const auto localModelIndex = localToScene.size();
			sceneToLocal[ found->second ] = localToScene.size();
			localToScene.push_back( (int) found->second );
		}
		else {
			// model does not exist in current scene
			localToScene.push_back( INVALID_INDEX );
		}
	}

	// TODO: fix this [10/21/2012 kirschan2]
	/*int getOrRegisterLocalModel( int sceneModelIndex ) {
		int localModelIndex = getLocalModelIndex( sceneModelIndex );
		if( localModelIndex == INVALID_INDEX ) {
			localModelIndex = localToScene.size();
			sceneToLocal[ sceneModelIndex ] =
		}
		return localModelIndex;
	}*/

	int getLocalModelIndex( int sceneModelIndex ) const {
		if( sceneModelIndex < sceneToLocal.size() ) {
			return sceneToLocal[ sceneModelIndex ];
		}
		else {
			return INVALID_INDEX;
		}
	}

	int getSceneModelIndex( int localModelIndex ) const {
		if( localModelIndex < localToScene.size() ) {
			return localToScene[ localModelIndex ];
		}
		else {
			return INVALID_INDEX;
		}
	}

	const std::string getSceneModelName( int sceneModelIndex ) const {
		return sceneModelNames[ sceneModelIndex ];
	}
};

typedef ProbeGenerator::Probe RawProbe;
typedef ProbeGenerator::Probes RawProbes;
typedef OptixProgramInterface::ProbeSample RawProbeSample;
typedef OptixProgramInterface::ProbeSamples RawProbeSamples;

// TODO: IO sucks as name [11/5/2012 kirschan2]
namespace IO {
	void loadRawQuery( const std::string &filename, RawProbes &probes, RawProbeSamples &probeSamples );
	void storeRawQuery( const std::string &filename, const RawProbes &probes, const RawProbeSamples &probeSamples );

	QueryResults loadQueryResults( const std::string &filename );
	void storeQueryResults( const std::string &filename, const QueryResults &results );
}

struct IDatabase {
	virtual void registerSceneModels( const std::vector< std::string > &modelNames ) = 0;

	virtual bool load( const std::string &filename ) = 0;
	virtual void store( const std::string &filename ) const = 0;

	virtual void clearAll() = 0;
	virtual void clear( int sceneModelIndex ) = 0;

	// TODO: add it should be straight-forward to change source to contain sceneName + obb if necessary [10/21/2012 kirschan2]
	virtual void addInstanceProbes(
		int sceneModelIndex,
		const Obb::Transformation &instanceTransformation,
		const float resolution,
		const RawProbes &probes,
		const RawProbeSamples &probeSamples
	) = 0;
	// compile the database in any way that is necessary before we can execute queries
	virtual void compile( int sceneModelIndex ) = 0;
	virtual void compileAll() = 0;

	// the query interface is implemented differently by every database
};


//////////////////////////////////////////////////////////////////////////


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
		int occlusion_integerTolerance,
		float colorLab_squaredTolerance,
		float distance_tolerance
	)
		: occlusion_integerTolerance( occlusion_integerTolerance )
		, colorLab_squaredTolerance( colorLab_squaredTolerance )
		, distance_tolerance( distance_tolerance )
	{
	}

	void setDefault() {
		occlusion_integerTolerance = int( 0.25 * OptixProgramInterface::numProbeSamples );
		colorLab_squaredTolerance = 25;
		distance_tolerance = 0.25f;
	}
};

typedef RawProbe DBProbe;

struct DBProbeSample : RawProbeSample {
	// sometimes we're lucky and we can compress probes
	int weight;
	int probeIndex;

	DBProbeSample() {}

	DBProbeSample( int probeIndex, const RawProbeSample &sample )
		: RawProbeSample( sample )
		, probeIndex( probeIndex )
		, weight( 1 )
	{
	}

	static __forceinline__ bool lexicographicalLess( const DBProbeSample &a, const DBProbeSample &b ) {
		return
				boost::make_tuple( a.occlusion, a.distance, a.colorLab.x, a.colorLab.y, a.colorLab.z )
			<
				boost::make_tuple( b.occlusion, b.distance, b.colorLab.x, a.colorLab.y, a.colorLab.z )
		;
	}

	static __forceinline__ bool less_byId( const DBProbeSample &a, const DBProbeSample &b ) {
		return a.probeIndex < b.probeIndex;
	}

	static __forceinline__ bool lexicographicalLess_startWithDistance( const DBProbeSample &a, const DBProbeSample &b ) {
		return
				boost::make_tuple( a.distance, a.colorLab.x, a.colorLab.y, a.colorLab.z )
			<
				boost::make_tuple( b.distance, b.colorLab.x, a.colorLab.y, a.colorLab.z )
		;
	}

	static __forceinline__ bool matchColor( const DBProbeSample &a, const DBProbeSample &b, const float squaredTolerance ) {
		Eigen::Vector3i colorDistance(
			a.colorLab.x - b.colorLab.x,
			a.colorLab.y - b.colorLab.y,
			a.colorLab.z - b.colorLab.z
			);
		if( colorDistance.squaredNorm() > squaredTolerance ) {
			return false;
		}
		return true;
	}

	static __forceinline__ bool matchDistance( const DBProbeSample &a, const DBProbeSample &b, const float tolerance ) {
		return fabs( a.distance - b.distance ) <= tolerance;
	}

	static __forceinline__ bool matchOcclusion( const DBProbeSample &a, const DBProbeSample &b, const int integerTolerance) {
		return abs( a.occlusion - b.occlusion ) <= integerTolerance;
	}

	static __forceinline__ bool matchOcclusionDistanceColor(
		const DBProbeSample &a,
		const DBProbeSample &b,
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

typedef std::vector< DBProbe > DBProbes;
typedef std::vector< DBProbeSample > DBProbeSamples;

// TODO: use this [10/27/2012 Andreas]
namespace ProbeSampleTransformation {
	inline DBProbeSamples transformSamples( const RawProbeSamples &rawProbeSamples ) {
		DBProbeSamples probeSamples;
		probeSamples.reserve( rawProbeSamples.size() );

		for( int probeIndex = 0 ; probeIndex < rawProbeSamples.size() ; ++probeIndex ) {
			const auto &rawProbeSample = rawProbeSamples[ probeIndex ];

			probeSamples.emplace_back( DBProbeSample( probeIndex, rawProbeSample ) );
		}

		return probeSamples;
	}
}

namespace ProbeHelpers {
	typedef std::vector< int > DirectionCounts;
	inline DirectionCounts countProbeDirections( const DBProbes &probes ) {
		DirectionCounts directionCounts( ProbeGenerator::getNumDirections() );
		for (auto probe = probes.begin() ; probe != probes.end() ; ++probe ) {
			directionCounts[ probe->directionIndex ]++;
		}
		return directionCounts;
	}
}

namespace CompressedDataset {
	inline void compress( size_t numInstances, size_t numProbes, DBProbeSamples &data, const ProbeContextToleranceV2 &pct ) {
		AUTO_TIMER_FUNCTION();

		AUTO_TIMER_BLOCK( "presorting" ) {
			// TODO: we know how the probe contests are interleaved, so we can just permute them
			boost::sort( data, DBProbeSample::less_byId );
		}

		AUTO_TIMER_BLOCK( "compressing" ) {
			DBProbeSamples compressedData;
			compressedData.reserve( data.size() );

			for( size_t probeIndex = 0 ; probeIndex < numProbes ; ++probeIndex ) {
				const auto probeSample_end = data.begin() + (probeIndex + 1) * numInstances;
				auto probeSample_mergeBegin = data.begin() + probeIndex * numInstances;
				while( probeSample_mergeBegin != probeSample_end ) {
					auto probeSample_mergeEnd = std::partition( probeSample_mergeBegin, probeSample_end,
						[probeSample_mergeBegin, pct] ( const DBProbeSample &sample ) {
							return DBProbeSample::matchOcclusionDistanceColor(
								*probeSample_mergeBegin,
								sample,
								pct.occlusion_integerTolerance,
								pct.distance_tolerance,
								pct.colorLab_squaredTolerance
							);
						}
					);

					auto probeSample_median = probeSample_mergeBegin + (probeSample_mergeEnd - probeSample_mergeBegin) / 2;
					// TODO: we can do this in place without a second vector [10/17/2012 kirschan2]
					compressedData.push_back( *probeSample_median );
					compressedData.back().weight = int( probeSample_mergeEnd - probeSample_mergeBegin );

					probeSample_mergeBegin = probeSample_mergeEnd;
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

struct ProbeCounter {
	float maxDistance;
	/* 3 bits for occlusion
	 * 3 bits for distance
	 * 3x4 bits for colors
	 * = 6+12 = 18 bits for lookup
	 * 2**18 * 4 = 1 MB
	 */
	static const int numBuckets = 1<<18;
	std::vector<unsigned> buckets;

	const unsigned getBucketIndex( const RawProbeSample &probeSample ) {
		union Index {
			struct {
			unsigned occlusion : 3;
			unsigned distance : 3;
			unsigned L : 4;
			unsigned a : 4;
			unsigned b : 4;
			};
			unsigned int value;
		} index;

		index.occlusion = clamp<int>( probeSample.occlusion * 8 / OptixProgramInterface::numProbeSamples, 0, 7 );
		index.distance = clamp<int>( probeSample.distance * 8.0 / maxDistance, 0, 7 );

		index.L = clamp<int>( probeSample.colorLab.x / 25, 0, 15 );
		index.a = clamp<int>( (probeSample.colorLab.y + 100) / 12.5f, 0, 15 );
		index.b = clamp<int>( (probeSample.colorLab.y + 100) / 12.5f, 0, 15 );

		return index.value;
	}

	ProbeCounter( float maxDistance = 0.0f )
		: buckets( numBuckets )
		, maxDistance( maxDistance )
	{
	}

	ProbeCounter( ProbeCounter &&other )
		: maxDistance( other.maxDistance )
		, buckets( std::move( buckets ) )
	{
	}
};

struct SampleQuantizer {
	/* 4 bits for a
	 * 4 bits for b
	 * 3 bits for L
	 * 3 bits for occlusion
	 * 5 bits for distance
	 * 19 bits adressing, 2**16 storage
	 */
	float maxDistance;
	static const int numBuckets = 1<<(4+4+3+3+5);

	typedef unsigned PackedSample;
	union Index {
		struct {
			unsigned distance: 5;
			unsigned occlusion: 3;
			unsigned b : 4;
			unsigned a : 4;
			unsigned L : 3;
		};
		PackedSample packedSample;
	};

	SampleQuantizer( float maxDistance = 5.0 ) 
		: maxDistance( maxDistance )
	{
	}

	short quantizeSample( const RawProbeSample &rawProbeSample ) const {
		Index index;
		index.packedSample = 0;
		index.distance = unsigned( rawProbeSample.distance / maxDistance ) * (1<<5);
		index.occlusion = unsigned( rawProbeSample.occlusion / OptixProgramInterface::numProbeSamples ) * (1<<3);

		index.a = clamp<int>( (rawProbeSample.colorLab.y + 100) / 2 * (1<<4), 0, (1<<4) - 1 );
		index.b = clamp<int>( (rawProbeSample.colorLab.z + 100) / 2 * (1<<4), 0, (1<<4) - 1 );
		index.L = clamp<int>( (rawProbeSample.colorLab.x) / 2 * (1<<3), 0, (1<<3) - 1 );

		return index.packedSample;
	}
};

struct SampleBitPlane {
	const static int numInts = SampleQuantizer::numBuckets >> 5;
	unsigned int plane[ numInts ];

	SampleBitPlane()
		: plane()
	{
	}

	bool test( SampleQuantizer::PackedSample packedSample ) const {
		return (plane[ packedSample >> 5 ] & (1<<(packedSample & 31))) != 0;
	}

	void set( SampleQuantizer::PackedSample packedSample ) {
		plane[ packedSample >> 5 ] |= 1<<(packedSample & 31);
	}

	void reset( SampleQuantizer::PackedSample packedSample ) {
		plane[ packedSample >> 5 ] &= ~(1<<(packedSample & 31));
	}

	void clear() {
		for( int i = 0 ; i < numInts ; i++ ) {
			plane[ i ] = 0;
		}
	}
};

struct LinearizedProbeSamples {
	int numProbes;
	std::vector<SampleQuantizer::PackedSample> samples;

	LinearizedProbeSamples()
		: numProbes()
	{}

	LinearizedProbeSamples( LinearizedProbeSamples &&other )
		: samples( std::move( other.samples ) )
		, numProbes( other.numProbes )
	{
	}

	LinearizedProbeSamples & operator =( LinearizedProbeSamples &&other ) {
		numProbes = other.numProbes;
		samples = std::move( other.samples );

		return *this;
	}

	void init( int numProbes, int numInstances ) {
		samples.clear();
		samples.reserve( numProbes * numInstances );
		this->numProbes = numProbes;
	}

	// unsorted!
	void push_back( const SampleQuantizer &quantizer, const RawProbeSample &rawProbeSample ) {
		samples.push_back( quantizer.quantizeSample( rawProbeSample ) );
	}

	void push_back( const SampleQuantizer &quantizer, const RawProbeSamples &rawProbeSamples ) {
		for( auto rawProbeSample = rawProbeSamples.begin() ; rawProbeSample != rawProbeSamples.end() ; ++rawProbeSample ) {
			samples.push_back( quantizer.quantizeSample( *rawProbeSample ) );
		}
	}

	void push_back( const SampleQuantizer &quantizer, const DBProbeSamples &dbProbeSamples ) {
		for( auto dbProbeSample = dbProbeSamples.begin() ; dbProbeSample != dbProbeSamples.end() ; ++dbProbeSample ) {
			samples.push_back( quantizer.quantizeSample( *dbProbeSample ) );
		}
	}

	int getProbeIndex( int sampleIndex ) const {
		return sampleIndex % numProbes;
	}
};

struct PartialProbeSamples {
	std::vector<std::pair<SampleQuantizer::PackedSample, unsigned>> samples;

	PartialProbeSamples()
	{}

	PartialProbeSamples( PartialProbeSamples &&other )
		: samples( std::move( other.samples ) )
	{
	}

	PartialProbeSamples & operator =( PartialProbeSamples &&other ) {
		samples = std::move( other.samples );

		return *this;
	}

	void init( int numProbes, int numInstances ) {
		samples.clear();
		samples.reserve( numProbes * numInstances );
	}

	// unsorted!
	void push_back( const SampleQuantizer &quantizer, int probeIndex, const RawProbeSample &rawProbeSample ) {
		samples.push_back( std::make_pair( quantizer.quantizeSample( rawProbeSample ), probeIndex ) );
	}

	int getSize() const {
		return samples.size();
	}

	int getProbeIndex( int sampleIndex ) const {
		return samples[ sampleIndex ].second;
	}

	SampleQuantizer::PackedSample getSample( int sampleIndex ) const {
		return samples[ sampleIndex ].first;
	}
};

namespace SampleBitPlaneHelper {
	void splatProbeSample( SampleBitPlane &targetPlane, const SampleQuantizer &quantizer, const RawProbeSample &rawProbeSample ) {
		targetPlane.set( quantizer.quantizeSample( rawProbeSample ) );
	}

	void splatProbeSamples( SampleBitPlane &targetPlane, const SampleQuantizer &quantizer, const RawProbeSamples &rawProbeSamples ) {
		for( auto rawProbeSample = rawProbeSamples.begin() ; rawProbeSample != rawProbeSamples.end() ; ++rawProbeSample ) {
			targetPlane.set( quantizer.quantizeSample( *rawProbeSample ) );
		}
	}

	void splatProbeSamples( SampleBitPlane &targetPlane, const SampleQuantizer &quantizer, const DBProbeSamples &dbProbeSamples ) {
		for( auto dbProbeSample = dbProbeSamples.begin() ; dbProbeSample != dbProbeSamples.end() ; ++dbProbeSample ) {
			targetPlane.set( quantizer.quantizeSample( *dbProbeSample ) );
		}
	}
}

struct SampleProbeIndexMap {
	// probeIndex is short for models
	typedef FlatMultiMap< SampleQuantizer::PackedSample, unsigned short, std::hash<SampleQuantizer::PackedSample>, unsigned short, unsigned > SampleMultiMap;
	SampleMultiMap sampleMultiMap;
	SampleMultiMap::Builder *builder;

	void startFilling( int numProbes, int numInstances ) {
		// we create a hash that could be perfect if the hash function was good
		const int numProbeSamples = numProbes * numInstances;
		sampleMultiMap.init( numProbeSamples, numProbeSamples );

		builder = new SampleMultiMap::Builder( sampleMultiMap );
	}

	void pushInstanceSample( const SampleQuantizer &quantizer, unsigned short probeIndex, const RawProbeSample &rawProbeSample ) {
		builder->insert( quantizer.quantizeSample( rawProbeSample ), probeIndex );
	}

	/*void pushInstanceSamples( const SampleQuantizer &quantizer, const RawProbeSamples &rawProbeSamples ) {
		const int rawProbeSamplesCount = (int) rawProbeSamples.size();
		for( int rawProbeIndex = 0 ; rawProbeIndex < rawProbeSamplesCount ; rawProbeIndex++ ) {
			const auto &rawProbeSample = rawProbeSamples[ rawProbeIndex ];

			builder->insert( quantizer.quantizeSample( rawProbeSample ), rawProbeIndex );
		}
	}*/

	void finishFilling() {
		builder->finish();
		delete [] builder;
	}

	SampleMultiMap::const_iterator_range lookup( SampleQuantizer::PackedSample packedSample ) const {
		return sampleMultiMap.equal_range( packedSample );
	}
};

struct ColorCounter {
	/* 3x4 bits for colors
	 * = 12 bits for lookup
	 * 2**12 * 4 = 16 KB
	 */
	static const int numBuckets = 1<<18;
	std::vector<unsigned> buckets;
	int totalNumSamples;
	float entropy;
	float totalMessageLength;

	static unsigned getBucketIndex( const RawProbeSample &probeSample ) {
		union Index {
			struct {
				unsigned L : 4;
				unsigned a : 4;
				unsigned b : 4;
			};
			unsigned value;
		} index;

		index.value = 0;

		index.L = clamp<int>( probeSample.colorLab.x / 25, 0, 15 );
		index.a = clamp<int>( (probeSample.colorLab.y + 100) / 12.5f, 0, 15 );
		index.b = clamp<int>( (probeSample.colorLab.y + 100) / 12.5f, 0, 15 );

		return index.value;
	}

	ColorCounter()
		: buckets( numBuckets )
		, totalNumSamples()
		, entropy()
		, totalMessageLength()
	{
	}

	ColorCounter( ColorCounter &&other )
		: buckets( std::move( other.buckets ) )
		, totalNumSamples( other.totalNumSamples )
		, entropy( other.entropy )
		, totalMessageLength( other.totalMessageLength )
	{
	}

	ColorCounter & operator = ( ColorCounter &&other ) {
		buckets = std::move( other.buckets );
		totalNumSamples = other.totalNumSamples;
		entropy = other.entropy;
		totalMessageLength = other.totalMessageLength;
	}

	void splat( const RawProbeSample &probeSample ) {
		const unsigned bucketIndex = getBucketIndex( probeSample );
		buckets[ bucketIndex ]++;
		totalNumSamples++;
	}

	void splatSamples( const RawProbeSamples &probeSamples ) {
		for( auto probeSample = probeSamples.begin() ; probeSample != probeSamples.end() ; probeSample++ ) {
			splat( *probeSample );
		}
	}

	float getAdjustedFrequency( int bucketIndex ) const {
		const int matches = buckets[ bucketIndex ];
		return (matches + 1.0) / (totalNumSamples + 2.0);
	}

	float getAdjustedFrequency( const RawProbeSample &probeSample ) const {
		const unsigned bucketIndex = getBucketIndex( probeSample );
		return getAdjustedFrequency( bucketIndex );
	}

	float getMessageLength( const RawProbeSample &probeSample ) const {
		return ::getMessageLength( getAdjustedFrequency( probeSample ) );
	}

	void calculateEntropy() {
		entropy = 0.0f;
		totalMessageLength = 0.0f;
		for( int bucketIndex = 0 ; bucketIndex < numBuckets ; bucketIndex++ ) {
			const int bucketSize = buckets[ bucketIndex ];
			if( bucketSize == 0 ) {
				continue;
			}

			const float frequency = float( bucketSize ) / float( totalNumSamples );
			const float adjustedFrequency = ( bucketSize + 1.0f ) / ( totalNumSamples + 2.0f );
			const float messageLength = ::getMessageLength( adjustedFrequency );
			totalMessageLength += bucketSize * messageLength;
			entropy += frequency * messageLength;
		}
	}

	void clear() {
		buckets.clear();
		buckets.resize( numBuckets );
		totalNumSamples = 0;
		entropy = 0.0f;
		totalMessageLength = 0.0f;
	}
};

// this dataset creates auxiliary structures automatically
// invariant: sorted and occlusionLowerBounds is correctlyset
struct IndexedProbeSamples {
	DBProbeSamples data;
	std::vector<int> occlusionLowerBounds;

	const DBProbeSamples &getProbeSamples() const {
		return data;
	}

	IndexedProbeSamples() {}

	IndexedProbeSamples( DBProbeSamples &&other ) :
		data( std::move( other ) ),
		occlusionLowerBounds()
	{
		sort();
		setOcclusionLowerBounds();
	}

	IndexedProbeSamples( IndexedProbeSamples &&other ) :
		data( std::move( other.data ) ),
		occlusionLowerBounds( std::move( other.occlusionLowerBounds ) )
	{
	}

	IndexedProbeSamples & operator = ( IndexedProbeSamples && other ) {
		data = std::move( other.data );
		occlusionLowerBounds = std::move( other.occlusionLowerBounds );

		return *this;
	}

	IndexedProbeSamples clone() const {
		IndexedProbeSamples cloned;
		cloned.data = data;
		cloned.occlusionLowerBounds = occlusionLowerBounds;
		return cloned;
	}

	int size() const {
		return (int) data.size();
	}

	typedef std::pair< int, int > IntRange;
	IntRange getOcclusionRange( int level ) const {
		return std::make_pair( occlusionLowerBounds[level], occlusionLowerBounds[ level + 1 ] );
	}

	// [leftLevel, rightLevel] (ie inclusive!)
	IntRange getOcclusionRange( int leftLevel, int rightLevel ) const {
		return std::make_pair( occlusionLowerBounds[leftLevel], occlusionLowerBounds[ rightLevel + 1 ] );
	}

#if 0
	struct MatcherController {
		void onMatch( int outerProbeSampleIndex, int innerProbeSampleIndex, const DBProbeSample &outer, const DBProbeSample &inner ) {
		}

		void onNewThreadStarted() {
		}
	};
#endif

	template< typename Controller >
	struct Matcher {
		const IndexedProbeSamples &probeSamplesInner;
		const IndexedProbeSamples &probeSamplesOuter;
		const ProbeContextTolerance &probeContextTolerance;
		Controller controller;

		Matcher( const IndexedProbeSamples &outer, const IndexedProbeSamples &inner, const ProbeContextTolerance &probeContextTolerance, Controller &&controller )
			: probeSamplesOuter( outer )
			, probeSamplesInner( inner )
			, probeContextTolerance( probeContextTolerance )
			, controller( std::forward<Controller>( controller ) )
		{
		}

		Matcher( const IndexedProbeSamples &outer, const IndexedProbeSamples &inner )
			: probeSamplesOuter( outer )
			, probeSamplesInner( inner )
			, probeContextTolerance( probeContextTolerance )
			, controller()
		{
		}

		void match() {
			if( probeSamplesOuter.size() == 0 || probeSamplesInner.size() == 0 ) {
				return;
			}

			// idea:
			//	use a binary search approach to generate only needed subranges

			// we can compare the different occlusion ranges against each other, after including the tolerance

			// TODO: is it better to make both ranges about equally big or not?
			// its better they are equal

			// assuming that the query set is smaller, we enlarge it, to have less items to sort than vice-versa
			// we could determine this at runtime...
			// if( sampledModels.size() > indexedProbeSamples.size() ) {...} else {...}
			const int occlusionTolerance = int( OptixProgramInterface::numProbeSamples * probeContextTolerance.occusionTolerance + 0.5 );

			// TODO: use a stack allocated array here? [9/27/2012 kirschan2]

			typedef std::pair< IntRange, IntRange > RangeJob;
			std::vector< RangeJob > rangeJobs;
			rangeJobs.reserve( OptixProgramInterface::numProbeSamples );
			for( int occulsionLevel = 0 ; occulsionLevel <= OptixProgramInterface::numProbeSamples ; occulsionLevel++ ) {
				const IntRange rangeOuter = probeSamplesOuter.getOcclusionRange( occulsionLevel );

				if( rangeOuter.first == rangeOuter.second ) {
					continue;
				}

				const int leftToleranceLevel = std::max( 0, occulsionLevel - occlusionTolerance );
				const int rightToleranceLevel = std::min( occulsionLevel + occlusionTolerance, OptixProgramInterface::numProbeSamples );
				for( int toleranceLevel = leftToleranceLevel ; toleranceLevel <= rightToleranceLevel ; toleranceLevel++ ) {
					const IntRange rangeInner = probeSamplesInner.getOcclusionRange( toleranceLevel );

					// is one of the ranges empty? if so, we don't need to check it at all
					if( rangeInner.first == rangeInner.second ) {
						continue;
					}

					// store the range for later
					rangeJobs.push_back( std::make_pair( rangeOuter, rangeInner ) );
				}
			}

			//AUTO_TIMER_BLOCK( "matching" )
			{
				using namespace Concurrency;
				parallel_for_each( rangeJobs.begin(), rangeJobs.end(),
					[&] ( const RangeJob &rangeJob ) {
						controller.onNewThreadStarted();

						matchSortedRanges(
							rangeJob.first,
							rangeJob.second
						);
					}
				);
			}
		}

		void matchSortedRanges(
			const IntRange &rangeOuter,
			const IntRange &rangeInner
		) {
			const float squaredColorTolerance = probeContextTolerance.colorLabTolerance * probeContextTolerance.colorLabTolerance;

			// assert: the range is not empty
			const int beginIndexOuter = rangeOuter.first;
			const int endIndexOuter = rangeOuter.second;
			int indexOuter = beginIndexOuter;

			const int beginIndexInner = rangeInner.first;
			const int endIndexInner = rangeInner.second;
			int indexInner = beginIndexInner;

			DBProbeSample probeSampleOuter = probeSamplesOuter.getProbeSamples()[ indexOuter ];
			for( ; indexOuter < endIndexOuter - 1 ; indexOuter++ ) {
				const DBProbeSample nextSampleOuter = probeSamplesOuter.getProbeSamples()[ indexOuter + 1 ];
				int nextIndexInner = indexInner;

				const float minDistance = probeSampleOuter.distance - probeContextTolerance.distanceTolerance;
				const float maxDistance = probeSampleOuter.distance + probeContextTolerance.distanceTolerance;
				const float minNextDistance = nextSampleOuter.distance - probeContextTolerance.distanceTolerance;

				for( ; indexInner < endIndexInner ; indexInner++ ) {
					const DBProbeSample probeSampleInner = probeSamplesInner.getProbeSamples()[ indexInner ];

					// distance too small?
					if( probeSampleInner.distance < minDistance ) {
						// then the next one is too far away as well
						nextIndexInner = indexInner + 1;
						continue;
					}

					// if nextIndexInner can't use this probe, the next overlapped sample might be the first one it likes
					if( probeSampleInner.distance < minNextDistance ) {
						// set it to the next ref sample
						nextIndexInner = indexInner + 1;
					}
					// else:
					//  nextIndexInner points to the first overlapped sample the next pure sample might match

					// are we past our interval
					if( probeSampleInner.distance > maxDistance ) {
						// enough for this probe, do the next
						break;
					}

					if( DBProbeSample::matchColor( probeSampleOuter, probeSampleInner, squaredColorTolerance ) ) {
						controller.onMatch( indexOuter, indexInner, probeSampleOuter, probeSampleInner );
					}
				}

				probeSampleOuter = nextSampleOuter;
				indexInner = nextIndexInner;
			}

			// process the last pure probe
			{
				const float minDistance = probeSampleOuter.distance - probeContextTolerance.distanceTolerance;
				const float maxDistance = probeSampleOuter.distance + probeContextTolerance.distanceTolerance;

				for( ; indexInner < endIndexInner ; indexInner++ ) {
					const DBProbeSample probeSampleInner = probeSamplesInner.getProbeSamples()[ indexInner ];

					// distance too small?
					if( probeSampleInner.distance < minDistance ) {
						continue;
					}

					// are we past our interval
					if( probeSampleInner.distance > maxDistance ) {
						// enough for this probe, we're done
						break;
					}

					if( DBProbeSample::matchColor( probeSampleOuter, probeSampleInner, squaredColorTolerance ) ) {
						controller.onMatch( indexOuter, indexInner, probeSampleOuter, probeSampleInner );
					}
				}
			}
		}
	};

private:
	void sort() {
		AUTO_TIMER_FUNCTION();

		boost::sort( data, DBProbeSample::lexicographicalLess );
	}

	void setOcclusionLowerBounds();

	SERIALIZER_FWD_FRIEND_EXTERN( ProbeContext::IndexedProbeSamples );

private:
	// better error messages than with boost::noncopyable
	IndexedProbeSamples( const IndexedProbeSamples &other );
	IndexedProbeSamples & operator = ( const IndexedProbeSamples &other );
};

namespace IndexedProbeSamplesHelper {
	inline std::vector< IndexedProbeSamples > createIndexedProbeSamplesByDirectionIndices(
		const DBProbes &probes,
		const DBProbeSamples &probeSamples
	) {
		std::vector< IndexedProbeSamples > indexedProbeSamplesByDirectionIndices( ProbeGenerator::getNumDirections() );

		const auto directionCounts = ProbeHelpers::countProbeDirections( probes );

		std::vector< DBProbeSamples > probeSamplesByDirectionIndices( ProbeGenerator::getNumDirections() );
		for( int directionIndex = 0 ; directionIndex < ProbeGenerator::getNumDirections() ; directionIndex++ ) {
			probeSamplesByDirectionIndices[ directionIndex ].reserve( directionCounts[ directionIndex ] );
		}

		const int probesCount = probes.size();
		for( int probeIndex = 0 ; probeIndex < probesCount ; probeIndex++ ) {
			const auto & probe = probes[ probeIndex ];
			const auto directionIndex = probe.directionIndex;

			probeSamplesByDirectionIndices[ directionIndex ].push_back( probeSamples[ probeIndex ] );
		}

		for( int directionIndex = 0 ; directionIndex < ProbeGenerator::getNumDirections() ; directionIndex++ ) {
			indexedProbeSamplesByDirectionIndices[ directionIndex ] = std::move( probeSamplesByDirectionIndices[ directionIndex ] );
		}

		return indexedProbeSamplesByDirectionIndices;
	}
}

struct SampledModel {
	struct SampledInstance {
		Obb::Transformation sourceTransformation;
		DBProbeSamples probeSamples;

		const DBProbeSamples &getProbeSamples() const {
			return probeSamples;
		}

		const Obb::Transformation &getSource() const {
			return sourceTransformation;
		}

		SampledInstance() {}

		SampledInstance( SampledInstance &&other )
			: sourceTransformation( std::move( other.sourceTransformation ) )
			, probeSamples( std::move( other.probeSamples ) )
		{
		}

		SampledInstance( Obb::Transformation sourceTransformation, DBProbeSamples &&probeSamples )
			: sourceTransformation( std::move( sourceTransformation ) )
			, probeSamples( std::move( probeSamples ) )
		{
		}

		SampledInstance & operator = ( SampledInstance &&other ) {
			sourceTransformation = std::move( other.sourceTransformation );
			probeSamples = std::move( other.probeSamples );

			return *this;
		}
	};
	typedef std::vector<SampledInstance> SampledInstances;

	void addInstanceProbes(
		const Obb::Transformation &sourceTransformation,
		float datasetResolution,
		const DBProbes &datasetProbes,
		DBProbeSamples &&probeSamples
	) {
		if( probes.size() != datasetProbes.size() || resolution != datasetResolution ) {
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

				clear();
			}

			probes = datasetProbes;

			// rotate probe positions
			AUTO_TIMER_BLOCK( "rotate probe postions for full queries" ) {
				for( int orientationIndex = 0 ; orientationIndex < ProbeGenerator::getNumOrientations() ; ++orientationIndex ) {
					rotatedProbePositions[ orientationIndex ] = ProbeGenerator::rotateProbePositions( probes, orientationIndex );
				}
			}

			resolution = datasetResolution;
		}

		instances.emplace_back( SampledInstance( sourceTransformation, std::move( probeSamples ) ) );
	}

	void clear() {
		instances.clear();

		mergedInstances = IndexedProbeSamples();

		mergedInstancesByDirectionIndex.clear();
		mergedInstancesByDirectionIndex.resize( ProbeGenerator::getNumDirections() );

		probes.clear();

		rotatedProbePositions.clear();
		rotatedProbePositions.resize( ProbeGenerator::getNumOrientations() );

		resolution = 0.f;
		modelColorCounter.clear();
	}

	SampleBitPlane sampleBitPlane;
	LinearizedProbeSamples linearizedProbeSamples;
	std::vector< std::pair< SampleBitPlane, SampleProbeIndexMap > > sampleProbeIndexMapByDirection;

private:
	ColorCounter modelColorCounter;
	SampledInstances instances;
	IndexedProbeSamples mergedInstances;

	std::vector< IndexedProbeSamples > mergedInstancesByDirectionIndex;

	DBProbes probes;
	std::vector< ProbeGenerator::ProbePositions > rotatedProbePositions;
	float resolution;

public:

	SampledModel()
		: resolution( 0.f )
	{
		mergedInstancesByDirectionIndex.resize( ProbeGenerator::getNumDirections() );
		sampleProbeIndexMapByDirection.resize( ProbeGenerator::getNumDirections() );

		rotatedProbePositions.resize( ProbeGenerator::getNumOrientations() );
	}

	SampledModel( SampledModel &&other )
		: instances( std::move( other.instances ) )
		, mergedInstances( std::move( other.mergedInstances ) )
		, mergedInstancesByDirectionIndex( std::move( other.mergedInstancesByDirectionIndex ) )
		, probes( std::move( other.probes ) )
		, rotatedProbePositions( std::move( other.rotatedProbePositions ) )
		, resolution( other.resolution )
		, modelColorCounter( std::move( other.modelColorCounter ) )

		, sampleBitPlane( std::move( other.sampleBitPlane ) )
		, linearizedProbeSamples( std::move( other.linearizedProbeSamples ) )
		, sampleProbeIndexMapByDirection( std::move( other.sampleProbeIndexMapByDirection ) )
	{}

	SampledModel & operator = ( SampledModel &&other ) {
		sampleBitPlane = std::move( sampleBitPlane );
		linearizedProbeSamples = std::move( linearizedProbeSamples );
		sampleProbeIndexMapByDirection = std::move( sampleProbeIndexMapByDirection );

		instances = std::move( other.instances );

		mergedInstances = std::move( other.mergedInstances );
		mergedInstancesByDirectionIndex = std::move( other.mergedInstancesByDirectionIndex );

		probes = std::move( other.probes );
		rotatedProbePositions = std::move( other.rotatedProbePositions );

		resolution = other.resolution;

		modelColorCounter = other.modelColorCounter;

		return *this;
	}

	// TODO: rename to compile
	void mergeInstances( const SampleQuantizer &quantizer, ColorCounter &colorCounter ) {
		// fast handling
		{
			sampleBitPlane.clear();
			linearizedProbeSamples.init( probes.size(), instances.size() );
			
			// now create a splat plane and a multi map for each direction
			for( int directionIndex = 0 ; directionIndex < ProbeGenerator::getNumDirections() ; directionIndex++ ) {
				auto &pair = sampleProbeIndexMapByDirection[ directionIndex ];
				pair.first.clear();
				pair.second.startFilling( probes.size(), instances.size() );
			}

			for( auto instance = instances.begin() ; instance < instances.end() ; instance++ ) {
				const auto probeSamples = instance->getProbeSamples();
				for( int probeIndex = 0 ; probeIndex < probes.size() ; probeIndex++ ) {
					const auto &probeSample = probeSamples[ probeIndex ];
					SampleBitPlaneHelper::splatProbeSample( sampleBitPlane, quantizer, probeSample );
					linearizedProbeSamples.push_back( quantizer, probeSample );

					auto &pair = sampleProbeIndexMapByDirection[ probes[ probeIndex ].directionIndex ];
					SampleBitPlaneHelper::splatProbeSample( pair.first, quantizer, probeSample );
					pair.second.pushInstanceSample( quantizer, probeIndex, probeSample );
				}
			}

			for( int directionIndex = 0 ; directionIndex < ProbeGenerator::getNumDirections() ; directionIndex++ ) {
				auto &pair = sampleProbeIndexMapByDirection[ directionIndex ];
				pair.second.finishFilling();
			}
		}


		{
			for( auto instance = instances.begin() ; instance < instances.end() ; instance++ ) {
				const auto probeSamples = instance->getProbeSamples();

				for( auto probeSample = probeSamples.begin() ; probeSample != probeSamples.end() ; probeSample++ ) {
					colorCounter.splat( *probeSample );
					modelColorCounter.splat( *probeSample );
				}
			}

			modelColorCounter.calculateEntropy();
		}

		if( instances.empty() ) {
			return;
		}
		else if( instances.size() == 1 ) {
			mergedInstances = IndexedProbeSamples( DBProbeSamples( instances.front().getProbeSamples() ) );
		}
		else {
			const int totalSize =  boost::accumulate(
				instances,
				0,
				[] ( int totalLength, const SampledInstance &instance) {
					return totalLength + int( instance.getProbeSamples().size() );
				}
			);

			DBProbeSamples mergedProbeSamples;
			mergedProbeSamples.reserve( totalSize );

			AUTO_TIMER_BLOCK( "push back all instances" ) {
				for( auto instance = instances.begin() ; instance != instances.end() ; ++instance ) {
					boost::push_back( mergedProbeSamples, instance->getProbeSamples() );
				}
			}

			// TODO: magic constants!!! [10/17/2012 kirschan2]
			//std::cout << OptixProgramInterface::numProbeSamples << "\n";
			/*ProbeContextToleranceV2 pctv2( int( 0.124f * OptixProgramInterface::numProbeSamples ), 1.0f, 0.25f * 0.95f );
			CompressedDataset::compress( instances.size(), probes.size(), mergedProbeSamples, pctv2 );*/
			mergedInstances = IndexedProbeSamples( std::move( mergedProbeSamples ) );
		}

		AUTO_TIMER_BLOCK( "push back all instances sorted by direction index" ) {
			const auto directionCounts = ProbeHelpers::countProbeDirections( probes );

			std::vector< DBProbeSamples > probeSamplesByDirectionIndex( ProbeGenerator::getNumDirections() );
			for( int directionIndex = 0 ; directionIndex < ProbeGenerator::getNumDirections() ; directionIndex++ ) {
				const int numProbesWithDirectionIndex = directionCounts[ directionIndex ] * (int) instances.size();
				probeSamplesByDirectionIndex[ directionIndex ].reserve( numProbesWithDirectionIndex );
			}

			const int probesCount = probes.size();
			for( int probeIndex = 0 ; probeIndex < probesCount ; probeIndex++ ) {
				const auto &probe = probes[ probeIndex ];
				const auto directionIndex = probe.directionIndex;

				auto &probeSamples = probeSamplesByDirectionIndex[ directionIndex ];
				for( int instanceIndex = 0 ; instanceIndex < instances.size() ; instanceIndex++ ) {
					probeSamples.push_back( instances[ instanceIndex ].getProbeSamples()[ probeIndex ] );
				}
			}

			for( int directionIndex = 0 ; directionIndex < ProbeGenerator::getNumDirections() ; directionIndex++ ) {
				mergedInstancesByDirectionIndex[ directionIndex ] = std::move( probeSamplesByDirectionIndex[ directionIndex ] );
			}
		}
	}

	bool isEmpty() const {
		return mergedInstances.size() == 0;
	}

	int uncompressedProbeSampleCount() const {
		return instances.size() * probes.size();
	}

	const SampledInstances & getInstances() const {
		return instances;
	}

	const IndexedProbeSamples & getMergedInstances() const {
		return mergedInstances;
	}

	const IndexedProbeSamples & getMergedInstancesByDirectionIndex( int directionIndex ) const {
		return mergedInstancesByDirectionIndex[ directionIndex ];
	}

	const ProbeGenerator::ProbePositions & getRotatedProbePositions( int orientationIndex ) const {
		return rotatedProbePositions[ orientationIndex ];
	}

	const DBProbes &getProbes() const {
		return probes;
	}

	const ColorCounter &getColorCounter() const {
		return modelColorCounter;
	}

	SERIALIZER_FWD_FRIEND_EXTERN( ProbeContext::SampledModel );

private:
	// better error messages than with boost::noncopyable
	SampledModel( const SampledModel &other );
	SampledModel & operator = ( const SampledModel &other );
};

struct ProbeDatabase : IDatabase {
	struct Settings {
		float maxDistance;
		float resolution;

		ProbeContextToleranceV2 compressionTolerance;
	};

	struct FastQuery;
	struct FastConfigurationQuery;

	struct Query;
	struct ImportanceQuery;
	struct FullQuery;
	struct ImportanceFullQuery;

	// TODO: fix the naming [10/15/2012 kirschan2]
	typedef std::vector<SampledModel> SampledModels;

	// the model index mapper is used to map local model ids to global ids at run-time depending on the scene
	ModelIndexMapper modelIndexMapper;
	// stored with the DB so that we can determine what a sampled model is
	std::vector< std::string > localModelNames;

	virtual void registerSceneModels( const std::vector< std::string > &modelNames );

	virtual bool load( const std::string &filename );
	virtual void store( const std::string &filename ) const;

	virtual void clear( int sceneModelIndex );
	virtual void clearAll();

	virtual void addInstanceProbes(
		int sceneModelIndex,
		const Obb::Transformation &sourceTransformation,
		const float resolution,
		const RawProbes &untransformedProbes,
		const RawProbeSamples &probeSamples
	);

	virtual void compile( int sceneModelIndex );
	virtual void compileAll( float maxDistance ) {
		sampleQuantizer.maxDistance = maxDistance;

		globalColorCounter.clear();
		for( auto sampledModel = sampledModels.begin() ; sampledModel != sampledModels.end() ; ++sampledModel ) {
			sampledModel->mergeInstances( sampleQuantizer, globalColorCounter );
		}
		globalColorCounter.calculateEntropy();
	}

	int getNumSampledModels() const {
		return (int) sampledModels.size();
	}

	int getSceneModelIndex( int localModelIndex ) const {
		return modelIndexMapper.getSceneModelIndex( localModelIndex );
	}

	bool isEmpty( int sceneModelIndex ) const {
		const int localModelIndex = modelIndexMapper.getLocalModelIndex( sceneModelIndex );
		if( localModelIndex == ModelIndexMapper::INVALID_INDEX ) {
			return true;
		}
		return sampledModels[ localModelIndex ].isEmpty();
	}

	const SampledModels & getSampledModels() const {
		return sampledModels;
	}

	ProbeDatabase() {}

private:
	SampledModels sampledModels;
	ColorCounter globalColorCounter;
	SampleQuantizer sampleQuantizer;

	SERIALIZER_FWD_FRIEND_EXTERN( ProbeContext::ProbeDatabase );
};

}

#include "probeDatabaseQueries.h"