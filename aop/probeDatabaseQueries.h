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

	void setQueryDataset( const RawProbeContexts &rawProbeContexts ) {
		this->indexedProbeContexts = IndexedProbeContexts( ProbeContextTransformation::transformContexts( rawProbeContexts ) );
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
	typedef IndexedProbeContexts::IntRange IntRange;
	typedef std::pair< IntRange, IntRange > OverlappedRange;

	DetailedQueryResult matchAgainst( int localSceneIndex, int sceneModelIndex ) {
		const auto &sampledModel = database.sampledModels[ localSceneIndex ];

		// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
		const IndexedProbeContexts &sampledModelProbeContexts = sampledModel.getMergedInstances();

		if( sampledModelProbeContexts.size() == 0 ) {
			return DetailedQueryResult( sceneModelIndex );
		}

		AUTO_TIMER_FOR_FUNCTION( boost::format( "id = %i, %i ref probes (%i query probes)" ) % sceneModelIndex % sampledModelProbeContexts.size() % indexedProbeContexts.size() );

		using namespace Concurrency;

		struct MatchController {
			combinable< int > numMatches;
			combinable< boost::dynamic_bitset<> > probesMatchedQueryVolume, probesMatchedSampledModel;

			int numProbeContextsSampledModel;
			int numProbeContextsQueryVolume;

			MatchController( int numProbeContextsSampledModel, int numProbeContextsQueryVolume )
				: numProbeContextsSampledModel( numProbeContextsSampledModel )
				, numProbeContextsQueryVolume( numProbeContextsQueryVolume )
			{
			}

			void onNewThreadStarted() {
				probesMatchedQueryVolume.local().resize( numProbeContextsQueryVolume );
				probesMatchedSampledModel.local().resize( numProbeContextsSampledModel );
			}

			void onMatch( int sampledModelProbeContextIndex, int queryProbeContextIndex, const DBProbeContext &sampledModelProbeContext, const DBProbeContext &queryProbeContext ) {
				numMatches.local() += sampledModelProbeContext.weight * queryProbeContext.weight;

				probesMatchedSampledModel.local()[ sampledModelProbeContextIndex ] = true;
				probesMatchedQueryVolume.local()[ queryProbeContextIndex ] = true;
			}
		};

		IndexedProbeContexts::Matcher< MatchController > matcher( sampledModelProbeContexts, indexedProbeContexts, probeContextTolerance, MatchController( sampledModelProbeContexts.size(), indexedProbeContexts.size() ) );
		AUTO_TIMER_BLOCK( "matching" ) {
			matcher.match();
		}

		boost::dynamic_bitset<> mergedProbeContextsSampledModel( sampledModelProbeContexts.size() ), mergedProbeContextsQueryVolume( indexedProbeContexts.size() );
		AUTO_TIMER_BLOCK( "combining matches" ) {
			matcher.controller.probesMatchedQueryVolume.combine_each(
				[&] ( const boost::dynamic_bitset<> &set ) {
					mergedProbeContextsQueryVolume |= set;
				}
			);

			matcher.controller.probesMatchedSampledModel.combine_each(
				[&] ( const boost::dynamic_bitset<> &set ) {
					mergedProbeContextsSampledModel |= set;
				}
			);
		}

		// query volumes are not compressed
		int numProbeContextsMatchedSampledModel = 0;
		for( int i = 0 ; i < mergedProbeContextsSampledModel.size() ; ++i ) {
			if( mergedProbeContextsSampledModel[ i ] ) {
				numProbeContextsMatchedSampledModel += sampledModelProbeContexts.getProbeContexts()[ i ].weight;
			}
		}

		const int numProbeContextsMatchedQueryVolume = (int) mergedProbeContextsQueryVolume.count();

		DetailedQueryResult detailedQueryResult( sceneModelIndex );
		detailedQueryResult.numMatches = matcher.controller.numMatches.combine( std::plus<int>() );

		detailedQueryResult.probeMatchPercentage = float( numProbeContextsMatchedSampledModel ) / sampledModel.uncompressedProbeContextCount();
		// query volumes are not compressed
		detailedQueryResult.queryMatchPercentage = float( numProbeContextsMatchedQueryVolume ) / indexedProbeContexts.size();

		detailedQueryResult.score = detailedQueryResult.probeMatchPercentage * detailedQueryResult.queryMatchPercentage;

		return detailedQueryResult;
	}

protected:
	const ProbeDatabase &database;

	IndexedProbeContexts indexedProbeContexts;

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

	void setQueryDataset( const DBProbes &probes, const RawProbeContexts &rawProbeContexts ) {
		this->probes = probes;
		this->indexedProbeContexts = IndexedProbeContexts( ProbeContextTransformation::transformContexts( rawProbeContexts ) );
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
		const IndexedProbeContexts &sampledModelProbeContexts = sampledModel.getMergedInstances();

		if( sampledModelProbeContexts.size() == 0 ) {
			return DetailedQueryResult( sceneModelIndex );
		}

		AUTO_TIMER_FOR_FUNCTION( boost::format( "id = %i, %i ref probes (%i query probes)" ) % sceneModelIndex % sampledModelProbeContexts.size() % indexedProbeContexts.size() );

		using namespace Concurrency;

		struct MatchController {
			combinable< int > numMatches;
			combinable< std::vector< float > > probesMatchedQueryVolume, probesMatchedSampledModel;

			const DBProbes &sampledModelProbes;
			const DBProbes &queryProbes;

			int numProbeContextsSampledModel;
			int numProbeContextsQueryVolume;

			MatchController( int numProbeContextsSampledModel, int numProbeContextsQueryVolume, const DBProbes &sampledModelProbes, const DBProbes &queryProbes )
				: numProbeContextsSampledModel( numProbeContextsSampledModel )
				, numProbeContextsQueryVolume( numProbeContextsQueryVolume )
				, sampledModelProbes( sampledModelProbes )
				, queryProbes( queryProbes )
			{
			}

			void onNewThreadStarted() {
				probesMatchedQueryVolume.local().resize( numProbeContextsQueryVolume );
				probesMatchedSampledModel.local().resize( numProbeContextsSampledModel );
			}

			void onMatch( int sampledModelProbeContextIndex, int queryProbeContextIndex, const DBProbeContext &sampledModelProbeContext, const DBProbeContext &queryProbeContext ) {
				numMatches.local() = sampledModelProbeContext.weight * queryProbeContext.weight;

				const float score = getMatchScore( sampledModelProbes[ sampledModelProbeContext.probeIndex ], queryProbes[ queryProbeContext.probeIndex ] );

				probesMatchedSampledModel.local()[ sampledModelProbeContextIndex ] = std::max( probesMatchedSampledModel.local()[ sampledModelProbeContext.probeIndex ], score * sampledModelProbeContext.weight );
				probesMatchedQueryVolume.local()[ queryProbeContextIndex ] = std::max( probesMatchedQueryVolume.local()[ queryProbeContextIndex ], score * queryProbeContext.weight );;
			}
		};

		IndexedProbeContexts::Matcher< MatchController > matcher(
			sampledModelProbeContexts,
			indexedProbeContexts,
			probeContextTolerance,
			MatchController( sampledModelProbeContexts.size(), indexedProbeContexts.size(), sampledModel.getProbes(), probes )
		);
		AUTO_TIMER_BLOCK( "matching" ) {
			matcher.match();
		}

		std::vector< float > mergedProbeContextsSampledModel( sampledModelProbeContexts.size() ), mergedProbeContextsQueryVolume( indexedProbeContexts.size() );
		AUTO_TIMER_BLOCK( "combining matches" ) {
			matcher.controller.probesMatchedQueryVolume.combine_each(
				[&] ( const std::vector< float > &matches ) {
					boost::transform( mergedProbeContextsQueryVolume, matches, mergedProbeContextsQueryVolume.begin(), std::max<float> );
				}
			);

			matcher.controller.probesMatchedSampledModel.combine_each(
				[&] ( const std::vector< float > &matches ) {
					boost::transform( mergedProbeContextsSampledModel, matches, mergedProbeContextsSampledModel.begin(), std::max<float> );
				}
			);
		}

		// query volumes are not compressed
		const float numProbeContextsMatchedSampledModel = boost::accumulate( mergedProbeContextsSampledModel, 0.0f, std::plus<float>() );
		const float numProbeContextsMatchedQueryVolume = boost::accumulate( mergedProbeContextsQueryVolume, 0.0f, std::plus<float>() );
		DetailedQueryResult detailedQueryResult( sceneModelIndex  );
		detailedQueryResult.numMatches = matcher.controller.numMatches.combine( std::plus<int>() );

		detailedQueryResult.probeMatchPercentage = float( numProbeContextsMatchedSampledModel ) / sampledModel.uncompressedProbeContextCount();
		// query volumes are not compressed
		detailedQueryResult.queryMatchPercentage = float( numProbeContextsMatchedQueryVolume ) / indexedProbeContexts.size();

		detailedQueryResult.score = detailedQueryResult.probeMatchPercentage * detailedQueryResult.queryMatchPercentage;

		return detailedQueryResult;
	}

protected:
	const ProbeDatabase &database;

	IndexedProbeContexts indexedProbeContexts;
	DBProbes probes;

	ProbeContextTolerance probeContextTolerance;

	DetailedQueryResults detailedQueryResults;
	QueryResults queryResults;
};
#endif