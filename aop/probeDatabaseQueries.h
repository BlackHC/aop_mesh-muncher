#pragma once

#include "probeDatabase.h"

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

		// NOTE: this can be easily parallelized
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
		AUTO_TIMER_BLOCK( "matching" ) {
			matcher.match();
		}

		boost::dynamic_bitset<> mergedProbeSamplesSampledModel( sampledModelProbeSamples.size() ), mergedProbeSamplesQueryVolume( indexedProbeSamples.size() );
		AUTO_TIMER_BLOCK( "combining matches" ) {
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

		return detailedQueryResult;
	}

protected:
	const ProbeDatabase &database;

	IndexedProbeSamples indexedProbeSamples;

	ProbeContextTolerance probeContextTolerance;

	DetailedQueryResults detailedQueryResults;
	QueryResults queryResults;
};

#if 1
struct ProbeDatabase::WeightedQuery {
	typedef std::shared_ptr<WeightedQuery> Ptr;

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

	WeightedQuery( const ProbeDatabase &database ) : database( database ) {}

	void setProbeContextTolerance( const ProbeContextTolerance &pct ) {
		probeContextTolerance = pct;
	}

	void setQueryDataset( const DBProbes &probes, const RawProbeSamples &rawProbeSamples ) {
		this->probes = probes;
		this->indexedProbeSamples = IndexedProbeSamples( ProbeSampleTransformation::transformSamples( rawProbeSamples ) );
	}

	void execute() {
		if( !queryResults.empty() ) {
			throw std::logic_error( "queryResults is not empty!" );
		}

		// NOTE: this can be easily parallelized
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
	static float getMatchScore( const DBProbe &sampledModelProbe, const DBProbe &queryProbe ) {
		const auto queryProbeDirection = Eigen::map( queryProbe.direction );
		const auto sampledModelProbeDirection = Eigen::map( sampledModelProbe.direction );

		//const float directionScore = (1.0 + queryProbeDirection.dot( sampledModelProbeDirection )) * 0.5;
		const float directionScore = queryProbeDirection.dot( sampledModelProbeDirection );
		if( directionScore <= 0.0 ) {
			return 0.0;
		}
		return directionScore;
		/*
		const auto queryProbePosition = Eigen::map( queryProbe.position );
		const auto sampledModelProbePosition = Eigen::map( sampledModelProbe.position );

		const float alpha = 0.001;
		const float beta = 0.00001;

		const Eigen::Vector3f delta = queryProbePosition - sampledModelProbePosition;
		const float deltaDot = delta.dot( sampledModelProbeDirection );
		const float shiftScore =
			std::max(
				0.0,
				1.0 - (alpha * fabs( deltaDot ) + beta * sqrt(delta.squaredNorm() - deltaDot*deltaDot))
			);
		return directionScore * shiftScore;*/
	}

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
			combinable< std::vector< float > > probesMatchedQueryVolume, probesMatchedSampledModel;

			const DBProbes &sampledModelProbes;
			const DBProbes &queryProbes;

			int numProbeSamplesSampledModel;
			int numProbeSamplesQueryVolume;

			MatchController( int numProbeSamplesSampledModel, int numProbeSamplesQueryVolume, const DBProbes &sampledModelProbes, const DBProbes &queryProbes )
				: numProbeSamplesSampledModel( numProbeSamplesSampledModel )
				, numProbeSamplesQueryVolume( numProbeSamplesQueryVolume )
				, sampledModelProbes( sampledModelProbes )
				, queryProbes( queryProbes )
			{
			}

			void onNewThreadStarted() {
				probesMatchedQueryVolume.local().resize( numProbeSamplesQueryVolume );
				probesMatchedSampledModel.local().resize( numProbeSamplesSampledModel );
			}

			void onMatch( int sampledModelProbeSampleIndex, int queryProbeSampleIndex, const DBProbeSample &sampledModelProbeSample, const DBProbeSample &queryProbeSample ) {
				numMatches.local() = sampledModelProbeSample.weight * queryProbeSample.weight;

				const float score = getMatchScore( sampledModelProbes[ sampledModelProbeSample.probeIndex ], queryProbes[ queryProbeSample.probeIndex ] );

				probesMatchedSampledModel.local()[ sampledModelProbeSampleIndex ] = std::max( probesMatchedSampledModel.local()[ sampledModelProbeSample.probeIndex ], score * sampledModelProbeSample.weight );
				probesMatchedQueryVolume.local()[ queryProbeSampleIndex ] = std::max( probesMatchedQueryVolume.local()[ queryProbeSampleIndex ], score * queryProbeSample.weight );;
			}
		};

		IndexedProbeSamples::Matcher< MatchController > matcher(
			sampledModelProbeSamples,
			indexedProbeSamples,
			probeContextTolerance,
			MatchController( sampledModelProbeSamples.size(), indexedProbeSamples.size(), sampledModel.getProbes(), probes )
		);
		AUTO_TIMER_BLOCK( "matching" ) {
			matcher.match();
		}

		std::vector< float > mergedProbeSamplesSampledModel( sampledModelProbeSamples.size() ), mergedProbeSamplesQueryVolume( indexedProbeSamples.size() );
		AUTO_TIMER_BLOCK( "combining matches" ) {
			matcher.controller.probesMatchedQueryVolume.combine_each(
				[&] ( const std::vector< float > &matches ) {
					boost::transform( mergedProbeSamplesQueryVolume, matches, mergedProbeSamplesQueryVolume.begin(), std::max<float> );
				}
			);

			matcher.controller.probesMatchedSampledModel.combine_each(
				[&] ( const std::vector< float > &matches ) {
					boost::transform( mergedProbeSamplesSampledModel, matches, mergedProbeSamplesSampledModel.begin(), std::max<float> );
				}
			);
		}

		// query volumes are not compressed
		const float numProbeSamplesMatchedSampledModel = boost::accumulate( mergedProbeSamplesSampledModel, 0.0f, std::plus<float>() );
		const float numProbeSamplesMatchedQueryVolume = boost::accumulate( mergedProbeSamplesQueryVolume, 0.0f, std::plus<float>() );
		DetailedQueryResult detailedQueryResult( sceneModelIndex  );
		detailedQueryResult.numMatches = matcher.controller.numMatches.combine( std::plus<int>() );

		detailedQueryResult.probeMatchPercentage = float( numProbeSamplesMatchedSampledModel ) / sampledModel.uncompressedProbeSampleCount();
		// query volumes are not compressed
		detailedQueryResult.queryMatchPercentage = float( numProbeSamplesMatchedQueryVolume ) / indexedProbeSamples.size();

		detailedQueryResult.score = detailedQueryResult.probeMatchPercentage * detailedQueryResult.queryMatchPercentage;

		return detailedQueryResult;
	}

protected:
	const ProbeDatabase &database;

	IndexedProbeSamples indexedProbeSamples;
	DBProbes probes;

	ProbeContextTolerance probeContextTolerance;

	DetailedQueryResults detailedQueryResults;
	QueryResults queryResults;
};
#endif