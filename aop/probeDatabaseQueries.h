#pragma once

#include "probeDatabase.h"
#include "boost\range\algorithm\max_element.hpp"

namespace ProbeContext {

struct ProbeDatabase::Query {
	typedef std::shared_ptr<Query> Ptr;

	struct DetailedQueryResult : QueryResult {
		float queryMatchPercentage;
		float probeMatchPercentage;
		int numMatches;

		DetailedQueryResult()
			: QueryResult()
			, queryMatchPercentage()
			, probeMatchPercentage()
			, numMatches()
		{
		}

		DetailedQueryResult( int sceneModelIndex )
			: QueryResult( sceneModelIndex )
			, queryMatchPercentage()
			, probeMatchPercentage()
			, numMatches()
		{
		}
	};
	typedef std::vector<DetailedQueryResult> DetailedQueryResults;

	Query( const ProbeDatabase &database ) : database( database ) {}
	
	void setQueryVolume( const Obb &queryVolume, float resolution ) {
		queryVolumeTransformation = queryVolume.transformation;
	}

	void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
		probeContextTolerance = pct;
	}

	void setQueryDataset( const RawProbeSamples &rawProbeSamples ) {
		this->indexedProbeSamples = IndexedProbeSamples( ProbeSampleTransformation::transformSamples( rawProbeSamples ) );
	}

	void execute() {
		if( !queryResults.empty() ) {
			throw std::logic_error( "queryResults is not empty!" );
		}

		detailedQueryResults.resize( database.sampledModels.size() );

		using namespace Concurrency;

		AUTO_TIMER_MEASURE() {
			int logScope = Log::getScope();

			parallel_for< int >(
				0,
				(int) database.sampledModels.size(),
				[&] ( int localModelIndex ) {
					Log::initThreadScope( logScope, 0 );

					detailedQueryResults[ localModelIndex ] = matchAgainst( localModelIndex, database.modelIndexMapper.getSceneModelIndex( localModelIndex ) );
				}
			);
		}

		boost::remove_erase_if( detailedQueryResults, [] ( const DetailedQueryResult &r ) { return !r.numMatches; });

		queryResults.resize( detailedQueryResults.size() );
		boost::transform( detailedQueryResults, queryResults.begin(), [] ( const DetailedQueryResult &r ) { return QueryResult( r ); } );
	}

	const QueryResults & getQueryResults() const {
		return queryResults;
	}

	const DetailedQueryResults & getDetailedQueryResults() const {
		return detailedQueryResults;
	}

protected:
	typedef IndexedProbeSamples::IntRange IntRange;
	typedef std::pair< IntRange, IntRange > OverlappedRange;

	DetailedQueryResult matchAgainst( int localSceneIndex, int sceneModelIndex ) {
		const auto &sampledModel = database.sampledModels[ localSceneIndex ];

		// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
		const IndexedProbeSamples &sampledModelProbeSamples = sampledModel.getMergedInstances();

		if( sampledModelProbeSamples.size() == 0 ) {
			return DetailedQueryResult( sceneModelIndex );
		}

		AUTO_TIMER_FOR_FUNCTION( boost::format( "id = %i, %i ref probes (%i query probes)" ) % sceneModelIndex % sampledModelProbeSamples.size() % indexedProbeSamples.size() );

		using namespace Concurrency;

		struct MatchController {
			combinable< int > numMatches;
			combinable< boost::dynamic_bitset<> > probesMatchedQueryVolume, probesMatchedSampledModel;

			int numProbeSamplesSampledModel;
			int numProbeSamplesQueryVolume;

			MatchController( int numProbeSamplesSampledModel, int numProbeSamplesQueryVolume )
				: numProbeSamplesSampledModel( numProbeSamplesSampledModel )
				, numProbeSamplesQueryVolume( numProbeSamplesQueryVolume )
			{
			}

			void onNewThreadStarted() {
				probesMatchedQueryVolume.local().resize( numProbeSamplesQueryVolume );
				probesMatchedSampledModel.local().resize( numProbeSamplesSampledModel );
			}

			void onMatch( int sampledModelProbeSampleIndex, int queryProbeSampleIndex, const DBProbeSample &sampledModelProbeSample, const DBProbeSample &queryProbeSample ) {
				numMatches.local() += sampledModelProbeSample.weight * queryProbeSample.weight;

				probesMatchedSampledModel.local()[ sampledModelProbeSampleIndex ] = true;
				probesMatchedQueryVolume.local()[ queryProbeSampleIndex ] = true;
			}
		};

		IndexedProbeSamples::Matcher< MatchController > matcher( sampledModelProbeSamples, indexedProbeSamples, probeContextTolerance, MatchController( sampledModelProbeSamples.size(), indexedProbeSamples.size() ) );
		//AUTO_TIMER_BLOCK( "matching" )
		{
			matcher.match();
		}

		boost::dynamic_bitset<> mergedProbeSamplesSampledModel( sampledModelProbeSamples.size() ), mergedProbeSamplesQueryVolume( indexedProbeSamples.size() );
		//AUTO_TIMER_BLOCK( "combining matches" )
		{
			matcher.controller.probesMatchedQueryVolume.combine_each(
				[&] ( const boost::dynamic_bitset<> &set ) {
					mergedProbeSamplesQueryVolume |= set;
				}
			);

			matcher.controller.probesMatchedSampledModel.combine_each(
				[&] ( const boost::dynamic_bitset<> &set ) {
					mergedProbeSamplesSampledModel |= set;
				}
			);
		}

		// query volumes are not compressed
		int numProbeSamplesMatchedSampledModel = 0;
		for( int i = 0 ; i < mergedProbeSamplesSampledModel.size() ; ++i ) {
			if( mergedProbeSamplesSampledModel[ i ] ) {
				numProbeSamplesMatchedSampledModel += sampledModelProbeSamples.getProbeSamples()[ i ].weight;
			}
		}

		const int numProbeSamplesMatchedQueryVolume = (int) mergedProbeSamplesQueryVolume.count();

		DetailedQueryResult detailedQueryResult( sceneModelIndex );
		detailedQueryResult.numMatches = matcher.controller.numMatches.combine( std::plus<int>() );

		detailedQueryResult.probeMatchPercentage = float( numProbeSamplesMatchedSampledModel ) / sampledModel.uncompressedProbeSampleCount();
		// query volumes are not compressed
		detailedQueryResult.queryMatchPercentage = float( numProbeSamplesMatchedQueryVolume ) / indexedProbeSamples.size();

		detailedQueryResult.score = detailedQueryResult.probeMatchPercentage * detailedQueryResult.queryMatchPercentage;

		detailedQueryResult.transformation = queryVolumeTransformation;

		return detailedQueryResult;
	}

protected:
	const ProbeDatabase &database;

	IndexedProbeSamples indexedProbeSamples;

	ProbeContextTolerance probeContextTolerance;

	DetailedQueryResults detailedQueryResults;
	QueryResults queryResults;

	Eigen::Affine3f queryVolumeTransformation;
};

struct ProbeDatabase::ImportanceQuery {
	typedef std::shared_ptr<ImportanceQuery> Ptr;

	struct DetailedQueryResult : QueryResult {
		float queryMatchPercentage;
		float probeMatchPercentage;
		int numMatches;

		DetailedQueryResult()
			: QueryResult()
			, queryMatchPercentage()
			, probeMatchPercentage()
			, numMatches()
		{
		}

		DetailedQueryResult( int sceneModelIndex )
			: QueryResult( sceneModelIndex )
			, queryMatchPercentage()
			, probeMatchPercentage()
			, numMatches()
		{
		}
	};
	typedef std::vector<DetailedQueryResult> DetailedQueryResults;

	ImportanceQuery( const ProbeDatabase &database ) : database( database ) {}

	void setQueryVolume( const Obb &queryVolume, float resolution ) {
		queryVolumeTransformation = queryVolume.transformation;
	}

	void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
		probeContextTolerance = pct;
	}

	void setQueryDataset( const RawProbeSamples &rawProbeSamples ) {
		queryColorCounter.splatSamples( rawProbeSamples );
		queryColorCounter.calculateEntropy();

		this->indexedProbeSamples = IndexedProbeSamples( ProbeSampleTransformation::transformSamples( rawProbeSamples ) );
	}

	void execute() {
		if( !queryResults.empty() ) {
			throw std::logic_error( "queryResults is not empty!" );
		}

		detailedQueryResults.resize( database.sampledModels.size() );

		using namespace Concurrency;

		AUTO_TIMER_MEASURE() {
			int logScope = Log::getScope();

			parallel_for< int >(
				0,
				(int) database.sampledModels.size(),
				[&] ( int localModelIndex ) {
					Log::initThreadScope( logScope, 0 );

					detailedQueryResults[ localModelIndex ] = matchAgainst( localModelIndex, database.modelIndexMapper.getSceneModelIndex( localModelIndex ) );
				}
			);
		}

		boost::remove_erase_if( detailedQueryResults, [] ( const DetailedQueryResult &r ) { return !r.numMatches; });

		queryResults.resize( detailedQueryResults.size() );
		boost::transform( detailedQueryResults, queryResults.begin(), [] ( const DetailedQueryResult &r ) { return QueryResult( r ); } );
	}

	const QueryResults & getQueryResults() const {
		return queryResults;
	}

	const DetailedQueryResults & getDetailedQueryResults() const {
		return detailedQueryResults;
	}

protected:
	typedef IndexedProbeSamples::IntRange IntRange;
	typedef std::pair< IntRange, IntRange > OverlappedRange;

	DetailedQueryResult matchAgainst( int localSceneIndex, int sceneModelIndex ) {
		const auto &sampledModel = database.sampledModels[ localSceneIndex ];

		// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
		const IndexedProbeSamples &sampledModelProbeSamples = sampledModel.getMergedInstances();

		if( sampledModelProbeSamples.size() == 0 ) {
			return DetailedQueryResult( sceneModelIndex );
		}

		AUTO_TIMER_FOR_FUNCTION( boost::format( "id = %i, %i ref probes (%i query probes)" ) % sceneModelIndex % sampledModelProbeSamples.size() % indexedProbeSamples.size() );

		using namespace Concurrency;

		struct MatchController {
			combinable< int > numMatches;
			combinable< boost::dynamic_bitset<> > probesMatchedQueryVolume, probesMatchedSampledModel;

			int numProbeSamplesSampledModel;
			int numProbeSamplesQueryVolume;

			MatchController( int numProbeSamplesSampledModel, int numProbeSamplesQueryVolume )
				: numProbeSamplesSampledModel( numProbeSamplesSampledModel )
				, numProbeSamplesQueryVolume( numProbeSamplesQueryVolume )
			{
			}

			void onNewThreadStarted() {
				probesMatchedQueryVolume.local().resize( numProbeSamplesQueryVolume );
				probesMatchedSampledModel.local().resize( numProbeSamplesSampledModel );
			}

			void onMatch( int sampledModelProbeSampleIndex, int queryProbeSampleIndex, const DBProbeSample &sampledModelProbeSample, const DBProbeSample &queryProbeSample ) {
				numMatches.local() += sampledModelProbeSample.weight * queryProbeSample.weight;

				probesMatchedSampledModel.local()[ sampledModelProbeSampleIndex ] = true;
				probesMatchedQueryVolume.local()[ queryProbeSampleIndex ] = true;
			}
		};

		IndexedProbeSamples::Matcher< MatchController > matcher( sampledModelProbeSamples, indexedProbeSamples, probeContextTolerance, MatchController( sampledModelProbeSamples.size(), indexedProbeSamples.size() ) );
		//AUTO_TIMER_BLOCK( "matching" )
		{
			matcher.match();
		}

		boost::dynamic_bitset<> mergedProbeSamplesSampledModel( sampledModelProbeSamples.size() ), mergedProbeSamplesQueryVolume( indexedProbeSamples.size() );
		//AUTO_TIMER_BLOCK( "combining matches" )
		{
			matcher.controller.probesMatchedQueryVolume.combine_each(
				[&] ( const boost::dynamic_bitset<> &set ) {
					mergedProbeSamplesQueryVolume |= set;
				}
			);

			matcher.controller.probesMatchedSampledModel.combine_each(
				[&] ( const boost::dynamic_bitset<> &set ) {
					mergedProbeSamplesSampledModel |= set;
				}
			);
		}

		// query volumes are not compressed
		float numProbeSamplesMatchedSampledModel = 0.0f;
		for( int i = 0 ; i < mergedProbeSamplesSampledModel.size() ; ++i ) {
			if( mergedProbeSamplesSampledModel[ i ] ) {
				const auto probeSample = sampledModelProbeSamples.getProbeSamples()[ i ];

				const float importanceWeight =
						database.globalColorCounter.getMessageLength( probeSample )
					+
						sampledModel.getColorCounter().getMessageLength( probeSample )
				;
				numProbeSamplesMatchedSampledModel += probeSample.weight * importanceWeight;
			}
		}

		float numProbeSamplesMatchedQueryVolume = 0.0f;
		for( int i = 0 ; i < mergedProbeSamplesQueryVolume.size() ; ++i ) {
			if( mergedProbeSamplesQueryVolume[ i ] ) {
				const auto probeSample = indexedProbeSamples.getProbeSamples()[ i ];

				const float importanceWeight =
						database.globalColorCounter.getMessageLength( probeSample )
					+
						queryColorCounter.getMessageLength( probeSample )
				;
				numProbeSamplesMatchedQueryVolume += probeSample.weight * importanceWeight;
			}
		}

		DetailedQueryResult detailedQueryResult( sceneModelIndex );
		detailedQueryResult.numMatches = matcher.controller.numMatches.combine( std::plus<int>() );

		const float avgTotalModelWeight = sampledModel.uncompressedProbeSampleCount() * database.globalColorCounter.entropy + sampledModel.getColorCounter().totalMessageLength;
		detailedQueryResult.probeMatchPercentage = float( numProbeSamplesMatchedSampledModel ) / avgTotalModelWeight;
		const float avgTotalQueryWeight = indexedProbeSamples.size() * database.globalColorCounter.entropy + queryColorCounter.totalMessageLength;
		// query volumes are not compressed
		detailedQueryResult.queryMatchPercentage = float( numProbeSamplesMatchedQueryVolume ) / avgTotalQueryWeight;

		detailedQueryResult.score = detailedQueryResult.probeMatchPercentage * detailedQueryResult.queryMatchPercentage;

		detailedQueryResult.transformation = queryVolumeTransformation;

		return detailedQueryResult;
	}

protected:
	const ProbeDatabase &database;

	IndexedProbeSamples indexedProbeSamples;
	ColorCounter queryColorCounter;

	ProbeContextTolerance probeContextTolerance;

	DetailedQueryResults detailedQueryResults;
	QueryResults queryResults;

	Eigen::Affine3f queryVolumeTransformation;
};

struct ProbeDatabase::FullQuery {
	struct DetailedQueryResult : QueryResult {
		std::vector< std::vector< int > > matchesByOrientation;

		DetailedQueryResult()
			: QueryResult()
			, matchesByOrientation( 24 )
		{
		}

		DetailedQueryResult( int sceneModelIndex )
			: QueryResult( sceneModelIndex )
			, matchesByOrientation( 24 )
		{
		}
	};
	typedef std::vector<DetailedQueryResult> DetailedQueryResults;

	FullQuery( const ProbeDatabase &database ) : database( database ) {}

	void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
		probeContextTolerance = pct;
	}

	void setQueryVolume( const Obb &queryVolume, float resolution ) {
		// TODO: I should wrap this in a new Grid structure [10/28/2012 Andreas]
		queryVolumeOffset = ProbeGenerator::getGridHalfExtent( queryVolume.size, resolution );
		queryVolumeSize = 2 * queryVolumeOffset + Eigen::Vector3i::Constant( 1 );
		queryVolumeTransformation = queryVolume.transformation;
		queryResolution = resolution;
	}

	// TODO: use && again!
	void setQueryDataset( const DBProbes &probes, const RawProbeSamples &rawProbeSamples ) {
		this->queryProbes = probes;
		this->indexedProbeSamplesByDirectionIndices =
			IndexedProbeSamplesHelper::createIndexedProbeSamplesByDirectionIndices( probes, ProbeSampleTransformation::transformSamples( rawProbeSamples ) );
	}

	void execute() {
		if( !queryResults.empty() ) {
			throw std::logic_error( "queryResults is not empty!" );
		}

		// NOTE: this can be easily parallelized
		detailedQueryResults.clear();
		detailedQueryResults.resize( database.sampledModels.size() );

		using namespace Concurrency;

		AUTO_TIMER_MEASURE() {
			int logScope = Log::getScope();

			parallel_for< int >(
				0,
				database.sampledModels.size(),
				[&] ( int id ) {
					Log::initThreadScope( logScope, 0 );

					detailedQueryResults[ id ] = matchAgainst( id, database.modelIndexMapper.getSceneModelIndex( id ) );
				}
			);
		}

		queryResults.resize( detailedQueryResults.size() );
		boost::transform( detailedQueryResults, queryResults.begin(), [] ( const DetailedQueryResult &r ) { return QueryResult( r ); } );
	}

	const DetailedQueryResults & getDetailedQueryResults() const {
		return detailedQueryResults;
	}

	const QueryResults & getQueryResults() const {
		return queryResults;
	}

protected:
	DetailedQueryResult matchAgainst( int localSceneIndex, int sceneModelIndex ) {
		const auto &sampledModel = database.sampledModels[ localSceneIndex ];

		AUTO_TIMER_FOR_FUNCTION(
			boost::format( "id = %i, %i ref probes (%i query probes)" )
			% sceneModelIndex
			% sampledModel.getMergedInstances().size()
			% queryProbes.size()
		);

		DetailedQueryResult detailedQueryResult( sceneModelIndex );

		for( int orientationIndex = 0 ; orientationIndex < ProbeGenerator::getNumOrientations() ; ++orientationIndex ) {
			using namespace Concurrency;

			struct MatchController {
				FullQuery &query;

				combinable< std::vector< int > > queryVolumeMatches;

				const ProbeGenerator::ProbePositions &modelProbePositions;
				const DBProbes &queryProbes;

				MatchController( FullQuery &query, const ProbeGenerator::ProbePositions &modelProbePositions, const DBProbes &queryProbes )
					: query( query )
					, modelProbePositions( modelProbePositions )
					, queryProbes( queryProbes )
				{
				}

				void onNewThreadStarted() {
					queryVolumeMatches.local().resize( query.queryVolumeSize.prod() );
				}

				void onMatch( int sampledModelProbeSampleIndex, int queryProbeSampleIndex, const DBProbeSample &sampledModelProbeSample, const DBProbeSample &queryProbeSample ) {
					// calculate the target position
					const Eigen::Vector3i targetPosition =
							queryProbes[ queryProbeSample.probeIndex ].position.cast<int>()
						-
							modelProbePositions[ sampledModelProbeSample.probeIndex ].cast<int>()
					;

					const Eigen::Vector3i targetCell = targetPosition + query.queryVolumeOffset;
					if( (targetCell.array() < 0).any() || (targetCell.array() >= query.queryVolumeSize.array()).any() ) {
						return;
					}
					const int targetCellIndex =
							targetCell.x()
						+
							targetCell.y() * query.queryVolumeSize.x()
						+
							targetCell.z() * query.queryVolumeSize.y() * query.queryVolumeSize.x()
					;
					queryVolumeMatches.local()[ targetCellIndex ] += sampledModelProbeSample.weight * queryProbeSample.weight;
				}
			};

			MatchController matchController( *this, sampledModel.getRotatedProbePositions( orientationIndex ), queryProbes );

			//AUTO_TIMER_BLOCK( "matching" )
			{
				const int *rotatedDirections = ProbeGenerator::getRotatedDirections( orientationIndex );
				for( int directionIndex = 0 ; directionIndex < ProbeGenerator::getNumDirections() ; directionIndex++ ) {
					IndexedProbeSamples::Matcher< MatchController& > matcher(
						sampledModel.getMergedInstancesByDirectionIndex( directionIndex ),
						indexedProbeSamplesByDirectionIndices[ rotatedDirections[ directionIndex ] ],
						probeContextTolerance,
						matchController
					);
					matcher.match();
				}
			}

			std::vector< int > mergedQueryVolumeMatches( queryVolumeSize.prod() );
			//AUTO_TIMER_BLOCK( "combining matches" )
			{
				matchController.queryVolumeMatches.combine_each(
					[&] ( const std::vector< int > &matches) {
						boost::transform( mergedQueryVolumeMatches, matches, mergedQueryVolumeMatches.begin(), std::plus<int>() );
					}
				);
			}

			auto maxElement = boost::max_element( mergedQueryVolumeMatches );
			const float score = float( *maxElement ) / sampledModel.getProbes().size();
			if( detailedQueryResult.score < score ) {
				detailedQueryResult.score = score;

				const int positionIndex = maxElement - mergedQueryVolumeMatches.begin();
				const int x = positionIndex % queryVolumeSize[0];
				const int y = (positionIndex / queryVolumeSize[0]) % queryVolumeSize[1];
				const int z = positionIndex / queryVolumeSize[0] / queryVolumeSize[1];

				detailedQueryResult.transformation =
						queryVolumeTransformation
					*	Eigen::Translation3f( queryResolution * (Eigen::Vector3f( x, y, z ) - queryVolumeOffset.cast<float>()) )
					*	Eigen::Affine3f( ProbeGenerator::getRotation( orientationIndex ) )
				;
			}

			detailedQueryResult.matchesByOrientation[ orientationIndex ] = std::move( mergedQueryVolumeMatches );
		}
		
		return detailedQueryResult;
	}

	// to be able to easily visualize stuff for now
public:
	const ProbeDatabase &database;

	std::vector< IndexedProbeSamples > indexedProbeSamplesByDirectionIndices;
	DBProbes queryProbes;

	ProbeContextTolerance probeContextTolerance;

	DetailedQueryResults detailedQueryResults;
	QueryResults queryResults;

	Eigen::Vector3i queryVolumeSize, queryVolumeOffset;
	Eigen::Affine3f queryVolumeTransformation;
	float queryResolution;
};

struct ProbeDatabase::ImportanceFullQuery {
	struct DetailedQueryResult : QueryResult {
		std::vector< std::vector< float > > matchesByOrientation;

		DetailedQueryResult()
			: QueryResult()
			, matchesByOrientation( 24 )
		{
		}

		DetailedQueryResult( int sceneModelIndex )
			: QueryResult( sceneModelIndex )
			, matchesByOrientation( 24 )
		{
		}
	};
	typedef std::vector<DetailedQueryResult> DetailedQueryResults;

	ImportanceFullQuery( const ProbeDatabase &database ) : database( database ) {}

	void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
		probeContextTolerance = pct;
	}

	void setQueryVolume( const Obb &queryVolume, float resolution ) {
		// TODO: I should wrap this in a new Grid structure [10/28/2012 Andreas]
		queryVolumeOffset = ProbeGenerator::getGridHalfExtent( queryVolume.size, resolution );
		queryVolumeSize = 2 * queryVolumeOffset + Eigen::Vector3i::Constant( 1 );
		queryVolumeTransformation = queryVolume.transformation;
		queryResolution = resolution;
	}

	// TODO: use && again!
	void setQueryDataset( const DBProbes &probes, const RawProbeSamples &rawProbeSamples ) {
		queryColorCounter.splatSamples( rawProbeSamples );
		queryColorCounter.calculateEntropy();

		this->queryProbes = probes;
		this->indexedProbeSamplesByDirectionIndices =
			IndexedProbeSamplesHelper::createIndexedProbeSamplesByDirectionIndices( probes, ProbeSampleTransformation::transformSamples( rawProbeSamples ) );
	}

	void execute() {
		if( !queryResults.empty() ) {
			throw std::logic_error( "queryResults is not empty!" );
		}

		// NOTE: this can be easily parallelized
		detailedQueryResults.clear();
		detailedQueryResults.resize( database.sampledModels.size() );

		using namespace Concurrency;

		AUTO_TIMER_MEASURE() {
			int logScope = Log::getScope();

			parallel_for< int >(
				0,
				database.sampledModels.size(),
				[&] ( int id ) {
					Log::initThreadScope( logScope, 0 );

					detailedQueryResults[ id ] = matchAgainst( id, database.modelIndexMapper.getSceneModelIndex( id ) );
				}
			);
		}

		queryResults.resize( detailedQueryResults.size() );
		boost::transform( detailedQueryResults, queryResults.begin(), [] ( const DetailedQueryResult &r ) { return QueryResult( r ); } );
	}

	const DetailedQueryResults & getDetailedQueryResults() const {
		return detailedQueryResults;
	}

	const QueryResults & getQueryResults() const {
		return queryResults;
	}

protected:
	DetailedQueryResult matchAgainst( int localSceneIndex, int sceneModelIndex ) {
		const auto &sampledModel = database.sampledModels[ localSceneIndex ];

		AUTO_TIMER_FOR_FUNCTION(
			boost::format( "id = %i, %i ref probes (%i query probes)" )
			% sceneModelIndex
			% sampledModel.getMergedInstances().size()
			% queryProbes.size()
		);

		DetailedQueryResult detailedQueryResult( sceneModelIndex );

		for( int orientationIndex = 0 ; orientationIndex < ProbeGenerator::getNumOrientations() ; ++orientationIndex ) {
			using namespace Concurrency;

			struct MatchController {
				ImportanceFullQuery &query;

				combinable< std::vector< float > > queryVolumeMatches;

				const ProbeGenerator::ProbePositions &modelProbePositions;
				const DBProbes &queryProbes;

				const ColorCounter &queryColorCounter;
				const ColorCounter &modelColorCounter;
				const ColorCounter &globalColorCounter;

				MatchController(
					ImportanceFullQuery &query,
					const ProbeGenerator::ProbePositions &modelProbePositions,
					const DBProbes &queryProbes,
					const ColorCounter &queryColorCounter,
					const ColorCounter &modelColorCounter,
					const ColorCounter &globalColorCounter
				)
					: query( query )
					, modelProbePositions( modelProbePositions )
					, queryProbes( queryProbes )
					, queryColorCounter( queryColorCounter )
					, modelColorCounter( modelColorCounter )
					, globalColorCounter( globalColorCounter )
				{
				}

				void onNewThreadStarted() {
					queryVolumeMatches.local().resize( query.queryVolumeSize.prod() );
				}

				void onMatch( int sampledModelProbeSampleIndex, int queryProbeSampleIndex, const DBProbeSample &sampledModelProbeSample, const DBProbeSample &queryProbeSample ) {
					// calculate the target position
					const Eigen::Vector3i targetPosition =
							queryProbes[ queryProbeSample.probeIndex ].position.cast<int>()
						-
							modelProbePositions[ sampledModelProbeSample.probeIndex ].cast<int>()
					;

					const Eigen::Vector3i targetCell = targetPosition + query.queryVolumeOffset;
					if( (targetCell.array() < 0).any() || (targetCell.array() >= query.queryVolumeSize.array()).any() ) {
						return;
					}
					const int targetCellIndex =
							targetCell.x()
						+
							targetCell.y() * query.queryVolumeSize.x()
						+
							targetCell.z() * query.queryVolumeSize.y() * query.queryVolumeSize.x()
					;

					const float importanceWeight =
							globalColorCounter.getMessageLength( sampledModelProbeSample )
						+
							modelColorCounter.getMessageLength( sampledModelProbeSample )
						+
							queryColorCounter.getMessageLength( queryProbeSample )
					;

					queryVolumeMatches.local()[ targetCellIndex ] += importanceWeight * sampledModelProbeSample.weight * queryProbeSample.weight;
				}
			};

			MatchController matchController(
				*this,
				sampledModel.getRotatedProbePositions( orientationIndex ),
				queryProbes,

				queryColorCounter,
				sampledModel.getColorCounter(),
				database.globalColorCounter
			);

			//AUTO_TIMER_BLOCK( "matching" )
			{
				const int *rotatedDirections = ProbeGenerator::getRotatedDirections( orientationIndex );
				for( int directionIndex = 0 ; directionIndex < ProbeGenerator::getNumDirections() ; directionIndex++ ) {
					IndexedProbeSamples::Matcher< MatchController& > matcher(
						sampledModel.getMergedInstancesByDirectionIndex( directionIndex ),
						indexedProbeSamplesByDirectionIndices[ rotatedDirections[ directionIndex ] ],
						probeContextTolerance,
						matchController
					);
					matcher.match();
				}
			}

			std::vector< float > mergedQueryVolumeMatches( queryVolumeSize.prod() );
			//AUTO_TIMER_BLOCK( "combining matches" )
			{
				matchController.queryVolumeMatches.combine_each(
					[&] ( const std::vector< float > &matches) {
						boost::transform( mergedQueryVolumeMatches, matches, mergedQueryVolumeMatches.begin(), std::plus<float>() );
					}
				);
			}

			auto maxElement = boost::max_element( mergedQueryVolumeMatches );
			const float normalizationFactor =
					(sampledModel.getProbes().size() + queryProbes.size()) * database.globalColorCounter.entropy
				+
					sampledModel.getColorCounter().totalMessageLength
				+
					queryColorCounter.totalMessageLength
			;

			const float score = float( *maxElement ) / sampledModel.getProbes().size();
			if( detailedQueryResult.score < score ) {
				detailedQueryResult.score = score;

				const int positionIndex = maxElement - mergedQueryVolumeMatches.begin();
				const int x = positionIndex % queryVolumeSize[0];
				const int y = (positionIndex / queryVolumeSize[0]) % queryVolumeSize[1];
				const int z = positionIndex / queryVolumeSize[0] / queryVolumeSize[1];

				detailedQueryResult.transformation =
						queryVolumeTransformation
					*	Eigen::Translation3f( queryResolution * (Eigen::Vector3f( x, y, z ) - queryVolumeOffset.cast<float>()) )
					*	Eigen::Affine3f( ProbeGenerator::getRotation( orientationIndex ) )
				;
			}

			detailedQueryResult.matchesByOrientation[ orientationIndex ] = std::move( mergedQueryVolumeMatches );
		}

		return detailedQueryResult;
	}

	// to be able to easily visualize stuff for now
public:
	const ProbeDatabase &database;

	std::vector< IndexedProbeSamples > indexedProbeSamplesByDirectionIndices;
	DBProbes queryProbes;
	ColorCounter queryColorCounter;

	ProbeContextTolerance probeContextTolerance;

	DetailedQueryResults detailedQueryResults;
	QueryResults queryResults;

	Eigen::Vector3i queryVolumeSize, queryVolumeOffset;
	Eigen::Affine3f queryVolumeTransformation;
	float queryResolution;
};
}