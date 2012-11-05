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

struct InstanceProbeDataset;
struct IndexedProbeSamples;
struct SampledModel;

SERIALIZER_FWD_EXTERN_DECL( InstanceProbeDataset )
SERIALIZER_FWD_EXTERN_DECL( IndexedProbeSamples )
SERIALIZER_FWD_EXTERN_DECL( SampledModel )

struct QueryResult {
	float score;
	int sceneModelIndex;

	Eigen::Vector3f position;
	Eigen::Quaternionf orientation;

	QueryResult()
		: score()
		, sceneModelIndex()
		, position()
		, orientation()
	{
	}

	QueryResult( int sceneModelIndex )
		: score()
		, sceneModelIndex( sceneModelIndex )
		, position()
		, orientation()
	{
	}

	QueryResult( float score, int sceneModelIndex, const Eigen::Vector3f &position, const Eigen::Quaternionf &orientation )
		: score( score )
		, sceneModelIndex( sceneModelIndex )
		, position( position )
		, orientation( orientation )
	{
	}

	static bool greaterByScoreAndModelIndex( const QueryResult &a, const QueryResult &b ) {
		return
				boost::make_tuple( a.score, a.sceneModelIndex )
			>
				boost::make_tuple( b.score, b.sceneModelIndex )
		;
	}
};

typedef std::vector< QueryResult > QueryResults;

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
		const Obb &sampleSource,
		const float resolution,
		const RawProbes &probes,
		const RawProbeSamples &probeSamples
	) = 0;
	// compile the database in any way that is necessary before we can execute queries
	virtual void compile( int sceneModelIndex ) = 0;
	virtual void compileAll() = 0;

	// the query interface is implemented differently by every database
};

#include <autoTimer.h>

#include <boost/lexical_cast.hpp>

//////////////////////////////////////////////////////////////////////////
#include <sort_permute_iter.h>
#include "boost/range/algorithm/sort.hpp"
#include <boost/iterator/counting_iterator.hpp>

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
		// TODO: cache the squared? [10/1/2012 kirschan2]
		if( colorDistance.squaredNorm() > squaredTolerance ) {
			return false;
		}
		return true;
	}

	static __forceinline__ bool matchDistance( const DBProbeSample &a, const DBProbeSample &b, const float tolerance ) {
		return fabs( a.distance - b.distance ) <= tolerance;
	}

	static __forceinline__ bool matchOcclusion( const DBProbeSample &a, const DBProbeSample &b, const int integerTolerance) {
		int absDelta = a.occlusion - b.occlusion;
		if( absDelta < 0 ) {
			absDelta = -absDelta;
		}
		return absDelta <= integerTolerance;
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

// this dataset creates auxiliary structures automatically
// invariant: sorted and occlusionLowerBounds is correctly set
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

	SERIALIZER_FWD_FRIEND_EXTERN( IndexedProbeSamples );

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
		Obb source;
		DBProbeSamples probeSamples;

		const DBProbeSamples &getProbeSamples() const {
			return probeSamples;
		}

		const Obb &getSource() const {
			return source;
		}

		SampledInstance() {}

		SampledInstance( SampledInstance &&other )
			: source( std::move( other.source ) )
			, probeSamples( std::move( other.probeSamples ) )
		{
		}

		SampledInstance( Obb source, DBProbeSamples &&probeSamples )
			: source( std::move( source ) )
			, probeSamples( std::move( probeSamples ) )
		{
		}

		SampledInstance & operator = ( SampledInstance &&other ) {
			source = std::move( other.source );
			probeSamples = std::move( other.probeSamples );

			return *this;
		}
	};
	typedef std::vector<SampledInstance> SampledInstances;

	void addInstanceProbes(
		const Obb &source,
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

		instances.emplace_back( SampledInstance( source, std::move( probeSamples ) ) );
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
	}

	SampledModel()
		: resolution( 0.f )
	{
		mergedInstancesByDirectionIndex.resize( ProbeGenerator::getNumDirections() );
		rotatedProbePositions.resize( ProbeGenerator::getNumOrientations() );
	}

	SampledModel( SampledModel &&other )
		: instances( std::move( other.instances ) )
		, mergedInstances( std::move( other.mergedInstances ) )
		, mergedInstancesByDirectionIndex( std::move( other.mergedInstancesByDirectionIndex ) )
		, probes( std::move( other.probes ) )
		, rotatedProbePositions( std::move( other.rotatedProbePositions ) )
		, resolution( other.resolution )
	{}

	SampledModel & operator = ( SampledModel &&other ) {
		instances = std::move( other.instances );
		
		mergedInstances = std::move( other.mergedInstances );
		mergedInstancesByDirectionIndex = std::move( other.mergedInstancesByDirectionIndex );
		
		probes = std::move( other.probes );
		rotatedProbePositions = std::move( other.rotatedProbePositions );

		resolution = other.resolution;

		return *this;
	}

	// TODO: rename to compile
	void mergeInstances() {
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
			ProbeContextToleranceV2 pctv2( int( 0.124f * OptixProgramInterface::numProbeSamples ), 1.0f, 0.25f * 0.95f );
			CompressedDataset::compress( instances.size(), probes.size(), mergedProbeSamples, pctv2 );
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
				const auto & probe = probes[ probeIndex ];
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

private:
	SampledInstances instances;
	IndexedProbeSamples mergedInstances;

	std::vector< IndexedProbeSamples > mergedInstancesByDirectionIndex;

	DBProbes probes;
	std::vector< ProbeGenerator::ProbePositions > rotatedProbePositions;
	float resolution;

	SERIALIZER_FWD_FRIEND_EXTERN( SampledModel );

private:
	// better error messages than with boost::noncopyable
	SampledModel( const SampledModel &other );
	SampledModel & operator = ( const SampledModel &other );
};

struct ProbeDatabase : IDatabase {
	struct Query;
	struct WeightedQuery;
	struct FullQuery;
	//struct OrientationQuery;

	// TODO: fix the naming [10/15/2012 kirschan2]
	typedef std::vector<SampledModel> SampledModels;

	ModelIndexMapper modelIndexMapper;

	std::vector< std::string > localModelNames;

	virtual void registerSceneModels( const std::vector< std::string > &modelNames );

	virtual bool load( const std::string &filename );
	virtual void store( const std::string &filename ) const;

	virtual void clear( int sceneModelIndex );
	virtual void clearAll();

	virtual void addInstanceProbes(
		int sceneModelIndex,
		const Obb &sampleSource,
		const float resolution,
		const RawProbes &untransformedProbes,
		const RawProbeSamples &probeSamples
	);

	virtual void compile( int sceneModelIndex );
	virtual void compileAll() {
		for( auto sampledModel = sampledModels.begin() ; sampledModel != sampledModels.end() ; ++sampledModel ) {
			sampledModel->mergeInstances();
		}
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
};

#include "probeDatabaseQueries.h"