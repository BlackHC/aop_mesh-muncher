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

typedef OptixProgramInterface::Probe RawProbe;
typedef OptixProgramInterface::Probes RawProbes;
typedef OptixProgramInterface::ProbeContext RawProbeContext;
typedef OptixProgramInterface::ProbeContexts RawProbeContexts;

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
		const RawProbes &untransformedProbes,
		const RawProbeContexts &probeContexts
	) = 0;
	// compile the database in any way that is necessary before we can execute queries
	virtual void compile( int sceneModelIndex ) = 0;
	virtual void compileAll() = 0;

	// the query interface is implemented differently by every database
};

struct InstanceProbeDataset;
struct IndexedProbeContexts;
struct SampledModel;

SERIALIZER_FWD_EXTERN_DECL( InstanceProbeDataset )
SERIALIZER_FWD_EXTERN_DECL( IndexedProbeContexts )
SERIALIZER_FWD_EXTERN_DECL( SampledModel )

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
		occlusion_integerTolerance = int( 0.25 * OptixProgramInterface::numProbeSamples );
		colorLab_squaredTolerance = 25;
		distance_tolerance = 0.25f;
	}
};

typedef OptixProgramInterface::Probe DBProbe;

struct DBProbeContext : OptixProgramInterface::ProbeContext {
	// sometimes we're lucky and we can compress probes
	int weight;
	int probeIndex;

	DBProbeContext() {}

	DBProbeContext( int probeIndex, const OptixProgramInterface::ProbeContext &context )
		: OptixProgramInterface::ProbeContext( context )
		, probeIndex( probeIndex )
		, weight( 1 )
	{
	}

	static __forceinline__ bool lexicographicalLess( const DBProbeContext &a, const DBProbeContext &b ) {
		return
			boost::make_tuple( a.hitCounter, a.distance, a.Lab.x, a.Lab.y, a.Lab.z )
			<
			boost::make_tuple( b.hitCounter, b.distance, b.Lab.x, a.Lab.y, a.Lab.z )
		;
	}

	static __forceinline__ bool less_byId( const DBProbeContext &a, const DBProbeContext &b ) {
		return a.probeIndex < b.probeIndex;
	}

	static __forceinline__ bool lexicographicalLess_startWithDistance( const DBProbeContext &a, const DBProbeContext &b ) {
		return
			boost::make_tuple( a.distance, a.Lab.x, a.Lab.y, a.Lab.z )
			<
			boost::make_tuple( b.distance, b.Lab.x, a.Lab.y, a.Lab.z )
		;
	}

	static __forceinline__ bool matchColor( const DBProbeContext &a, const DBProbeContext &b, const float squaredTolerance ) {
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

	static __forceinline__ bool matchDistance( const DBProbeContext &a, const DBProbeContext &b, const float tolerance ) {
		return fabs( a.distance - b.distance ) <= tolerance;
	}

	static __forceinline__ bool matchOcclusion( const DBProbeContext &a, const DBProbeContext &b, const int integerTolerance) {
		int absDelta = a.hitCounter - b.hitCounter;
		if( absDelta < 0 ) {
			absDelta = -absDelta;
		}
		return absDelta <= integerTolerance;
	}

	static __forceinline__ bool matchOcclusionDistanceColor(
		const DBProbeContext &a,
		const DBProbeContext &b,
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
typedef std::vector< DBProbeContext > DBProbeContexts;

namespace CompressedDataset {
	inline void compress( size_t numInstances, size_t numProbes, DBProbeContexts &data, const ProbeContextToleranceV2 &pct ) {
		AUTO_TIMER_FUNCTION();

		AUTO_TIMER_BLOCK( "presorting" ) {
			// TODO: we know how the probe contests are interleaved, so we can just permute them
			boost::sort( data, DBProbeContext::less_byId );
		}

		AUTO_TIMER_BLOCK( "compressing" ) {
			DBProbeContexts compressedData;
			compressedData.reserve( data.size() );

			for( size_t probeIndex = 0 ; probeIndex < numProbes ; ++probeIndex ) {
				const auto probeContext_end = data.begin() + (probeIndex + 1) * numInstances;
				auto probeContext_mergeBegin = data.begin() + probeIndex * numInstances;
				while( probeContext_mergeBegin != probeContext_end ) {
					auto probeContext_mergeEnd = std::partition( probeContext_mergeBegin, probeContext_end,
						[probeContext_mergeBegin, pct] ( const DBProbeContext &context ) {
							return DBProbeContext::matchOcclusionDistanceColor(
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
					compressedData.back().weight = int( probeContext_mergeEnd - probeContext_mergeBegin );

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

// this dataset creates auxiliary structures automatically
// invariant: sorted and hitCounterLowerBounds is correctly set
struct IndexedProbeContexts {
	DBProbeContexts data;
	std::vector<int> hitCounterLowerBounds;

	const DBProbeContexts &getProbeContexts() const {
		return data;
	}

	IndexedProbeContexts() {}

	IndexedProbeContexts( DBProbeContexts &&other ) :
		data( std::move( other ) ),
		hitCounterLowerBounds()
	{
		sort();
		setHitCounterLowerBounds();
	}

	IndexedProbeContexts( IndexedProbeContexts &&other ) :
		data( std::move( other.data ) ),
		hitCounterLowerBounds( std::move( other.hitCounterLowerBounds ) )
	{
	}

	IndexedProbeContexts & operator = ( IndexedProbeContexts && other ) {
		data = std::move( other.data );
		hitCounterLowerBounds = std::move( other.hitCounterLowerBounds );

		return *this;
	}

	IndexedProbeContexts clone() const {
		IndexedProbeContexts cloned;
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

#if 0
	struct MatcherController {
		void onMatch( int outerProbeContextIndex, int innerProbeContextIndex, const DBProbeContext &outer, const DBProbeContext &inner ) {
		}

		void onNewThreadStarted() {
		}
	};
#endif

	template< typename Controller >
	struct Matcher {
		const IndexedProbeContexts &probeContextsInner;
		const IndexedProbeContexts &probeContextsOuter;
		const ProbeContextTolerance &probeContextTolerance;
		Controller controller;

		Matcher( const IndexedProbeContexts &outer, const IndexedProbeContexts &inner, const ProbeContextTolerance &probeContextTolerance, Controller &&controller )
			: probeContextsOuter( outer )
			, probeContextsInner( inner )
			, probeContextTolerance( probeContextTolerance )
			, controller( controller )
		{
		}

		Matcher( const IndexedProbeContexts &outer, const IndexedProbeContexts &inner )
			: probeContextsOuter( outer )
			, probeContextsInner( inner )
			, probeContextTolerance( probeContextTolerance )
			, controller()
		{
		}

		void match() {
			if( probeContextsOuter.size() == 0 || probeContextsInner.size() == 0 ) {
				return;
			}

			// idea:
			//	use a binary search approach to generate only needed subranges

			// we can compare the different occlusion ranges against each other, after including the tolerance

			// TODO: is it better to make both ranges about equally big or not?
			// its better they are equal

			// assuming that the query set is smaller, we enlarge it, to have less items to sort than vice-versa
			// we could determine this at runtime...
			// if( idDatasets.size() > indexedProbeContexts.size() ) {...} else {...}
			const int occlusionTolerance = int( OptixProgramInterface::numProbeSamples * probeContextTolerance.occusionTolerance + 0.5 );

			// TODO: use a stack allocated array here? [9/27/2012 kirschan2]

			typedef std::pair< IntRange, IntRange > RangeJob;
			std::vector< RangeJob > rangeJobs;
			rangeJobs.reserve( OptixProgramInterface::numProbeSamples );
			for( int occulsionLevel = 0 ; occulsionLevel <= OptixProgramInterface::numProbeSamples ; occulsionLevel++ ) {
				const IntRange rangeOuter = probeContextsOuter.getOcclusionRange( occulsionLevel );

				if( rangeOuter.first == rangeOuter.second ) {
					continue;
				}

				const int leftToleranceLevel = std::max( 0, occulsionLevel - occlusionTolerance );
				const int rightToleranceLevel = std::min( occulsionLevel + occlusionTolerance, OptixProgramInterface::numProbeSamples );
				for( int toleranceLevel = leftToleranceLevel ; toleranceLevel <= rightToleranceLevel ; toleranceLevel++ ) {
					const IntRange rangeInner = probeContextsInner.getOcclusionRange( toleranceLevel );

					// is one of the ranges empty? if so, we don't need to check it at all
					if( rangeInner.first == rangeInner.second ) {
						continue;
					}

					// store the range for later
					rangeJobs.push_back( std::make_pair( rangeOuter, rangeInner ) );
				}
			}

			AUTO_TIMER_BLOCK( "matching" ) {
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

			DBProbeContext probeContextOuter = probeContextsOuter.getProbeContexts()[ indexOuter ];
			for( ; indexOuter < endIndexOuter - 1 ; indexOuter++ ) {
				const DBProbeContext nextContextOuter = probeContextsOuter.getProbeContexts()[ indexOuter + 1 ];
				int nextIndexInner = indexInner;

				const float minDistance = probeContextOuter.distance - probeContextTolerance.distanceTolerance;
				const float maxDistance = probeContextOuter.distance + probeContextTolerance.distanceTolerance;
				const float minNextDistance = nextContextOuter.distance - probeContextTolerance.distanceTolerance;

				for( ; indexInner < endIndexInner ; indexInner++ ) {
					const DBProbeContext probeContextInner = probeContextsInner.getProbeContexts()[ indexInner ];

					// distance too small?
					if( probeContextInner.distance < minDistance ) {
						// then the next one is too far away as well
						nextIndexInner = indexInner + 1;
						continue;
					}

					// if nextIndexInner can't use this probe, the next overlapped context might be the first one it likes
					if( probeContextInner.distance < minNextDistance ) {
						// set it to the next ref context
						nextIndexInner = indexInner + 1;
					}
					// else:
					//  nextIndexInner points to the first overlapped context the next pure context might match

					// are we past our interval
					if( probeContextInner.distance > maxDistance ) {
						// enough for this probe, do the next
						break;
					}

					if( DBProbeContext::matchColor( probeContextOuter, probeContextInner, squaredColorTolerance ) ) {
						controller.onMatch( indexOuter, indexInner, probeContextOuter, probeContextInner );
					}
				}

				probeContextOuter = nextContextOuter;
				indexInner = nextIndexInner;
			}

			// process the last pure probe
			{
				const float minDistance = probeContextOuter.distance - probeContextTolerance.distanceTolerance;
				const float maxDistance = probeContextOuter.distance + probeContextTolerance.distanceTolerance;

				for( ; indexInner < endIndexInner ; indexInner++ ) {
					const DBProbeContext probeContextInner = probeContextsInner.getProbeContexts()[ indexInner ];

					// distance too small?
					if( probeContextInner.distance < minDistance ) {
						continue;
					}

					// are we past our interval
					if( probeContextInner.distance > maxDistance ) {
						// enough for this probe, we're done
						break;
					}

					if( DBProbeContext::matchColor( probeContextOuter, probeContextInner, squaredColorTolerance ) ) {
						controller.onMatch( indexOuter, indexInner, probeContextOuter, probeContextInner );
					}
				}
			}
		}
	};

private:
	void sort() {
		AUTO_TIMER_FUNCTION();

		boost::sort( data, DBProbeContext::lexicographicalLess );
	}

	void setHitCounterLowerBounds();

	SERIALIZER_FWD_FRIEND_EXTERN( IndexedProbeContexts )
private:
	// better error messages than with boost::noncopyable
	IndexedProbeContexts( const IndexedProbeContexts &other );
	IndexedProbeContexts & operator = ( const IndexedProbeContexts &other );
};

struct SampledModel {
	struct SampledInstance {
		Obb source;
		DBProbeContexts probeContexts;

		const DBProbeContexts &getProbeContexts() const {
			return probeContexts;
		}

		const Obb &getSource() const {
			return source;
		}

		SampledInstance() {}

		SampledInstance( SampledInstance &&other )
			: source( std::move( other.source ) )
			, probeContexts( std::move( other.probeContexts ) )
		{
		}

		SampledInstance( Obb source, DBProbeContexts &&probeContexts )
			: source( std::move( source ) )
			, probeContexts( std::move( probeContexts ) )
		{
		}

		SampledInstance & operator = ( SampledInstance &&other ) {
			source = std::move( other.source );
			probeContexts = std::move( other.probeContexts );

			return *this;
		}
	};
	typedef std::vector<SampledInstance> SampledInstances;

	void addInstances( const DBProbes &datasetProbes, const Obb &source, DBProbeContexts &&probeContexts ) {
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
				mergedInstances = IndexedProbeContexts();
			}

			probes = datasetProbes;
		}

		instances.emplace_back( SampledInstance( source, std::move( probeContexts ) ) );
	}

	void clear() {
		instances.clear();
		mergedInstances = IndexedProbeContexts();
		probes.clear();
	}

	SampledModel()
	{}

	SampledModel( SampledModel &&other )
		: instances( std::move( other.instances ) )
		, mergedInstances( std::move( other.mergedInstances ) )
		, probes( std::move( other.probes ) )
	{}

	SampledModel & operator = ( SampledModel &&other ) {
		instances = std::move( other.instances );
		mergedInstances = std::move( other.mergedInstances );
		probes = std::move( other.probes );

		return *this;
	}

	// TODO: rename to compile
	void mergeInstances() {
		if( instances.empty() ) {
			return;
		}
		else if( instances.size() == 1 ) {
			mergedInstances = IndexedProbeContexts( DBProbeContexts( instances.front().getProbeContexts() ) );
		}
		else {
			const int totalSize =  boost::accumulate(
				instances,
				0,
				[] ( int totalLength, const SampledInstance &instance) {
					return totalLength + int( instance.getProbeContexts().size() );
				}
			);

			DBProbeContexts probeContexts;
			probeContexts.reserve( totalSize );

			AUTO_TIMER_BLOCK( "push back all instances" ) {
				for( auto instance = instances.begin() ; instance != instances.end() ; ++instance ) {
					boost::push_back( probeContexts, instance->getProbeContexts() );
				}
			}

			// TODO: magic constants!!! [10/17/2012 kirschan2]
			//CompressedDataset::compress( instances.size(), probes.size(), probeContexts, ProbeContextToleranceV2( int( 0.124f * OptixProgramInterface::numProbeSamples ), 1.0f, 0.25f * 0.95f ) );

			mergedInstances = IndexedProbeContexts( std::move( probeContexts ) );
		}
	}

	bool isEmpty() const {
		return mergedInstances.size() == 0;
	}

	int uncompressedProbeContextCount() const {
		return instances.size() * probes.size();
	}

	const SampledInstances & getInstances() const {
		return instances;
	}

	const IndexedProbeContexts & getMergedInstances() const {
		return mergedInstances;
	}

	const DBProbes &getProbes() const {
		return probes;
	}

private:
	SampledInstances instances;
	IndexedProbeContexts mergedInstances;

	DBProbes probes;

	SERIALIZER_FWD_FRIEND_EXTERN( SampledModel )

private:
	// better error messages than with boost::noncopyable
	SampledModel( const SampledModel &other );
	SampledModel & operator = ( const SampledModel &other );
};

struct ProbeDatabase : IDatabase {
	struct Query;
	struct WeightedQuery;

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
		const RawProbes &untransformedProbes,
		const RawProbeContexts &probeContexts
	);

	virtual void compile( int sceneModelIndex );
	virtual void compileAll() {
		for( auto idDataset = sampledModels.begin() ; idDataset != sampledModels.end() ; ++idDataset ) {
			idDataset->mergeInstances();
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

	static DBProbeContexts transformContexts( const RawProbeContexts &rawProbeContexts ) {
		DBProbeContexts probeContexts;
		probeContexts.reserve( rawProbeContexts.size() );

		for( int probeIndex = 0 ; probeIndex < rawProbeContexts.size() ; ++probeIndex ) {
			const auto &rawProbeContext = rawProbeContexts[ probeIndex ];

			probeContexts.emplace_back( DBProbeContext( probeIndex, rawProbeContext ) );
		}

		return probeContexts;
	}

private:
	// TODO: rename [10/22/2012 kirschan2]
	SampledModels sampledModels;
};

#include "probeDatabaseQueries.h"