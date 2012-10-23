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
		this->indexedProbeContexts = IndexedProbeContexts( ProbeDatabase::transformContexts( rawProbeContexts ) );
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
		// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
		const IndexedProbeContexts &idDataset = database.sampledModels[ localSceneIndex ].getMergedInstances();

		if( idDataset.size() == 0 ) {
			return DetailedQueryResult( sceneModelIndex );
		}

		AUTO_TIMER_FOR_FUNCTION( boost::format( "id = %i, %i ref probes (%i query probes)" ) % sceneModelIndex % idDataset.size() % indexedProbeContexts.size() );

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

		std::vector< OverlappedRange > overlappedRanges;
		overlappedRanges.reserve( OptixProgramInterface::numProbeSamples );
		for( int occulsionLevel = 0 ; occulsionLevel <= OptixProgramInterface::numProbeSamples ; occulsionLevel++ ) {
			const IndexedProbeContexts::IntRange queryRange = indexedProbeContexts.getOcclusionRange( occulsionLevel );

			if( queryRange.first == queryRange.second ) {
				continue;
			}

			const int leftToleranceLevel = std::max( 0, occulsionLevel - occlusionTolerance );
			const int rightToleranceLevel = std::min( occulsionLevel + occlusionTolerance, OptixProgramInterface::numProbeSamples );
			for( int idToleranceLevel = leftToleranceLevel ; idToleranceLevel <= rightToleranceLevel ; idToleranceLevel++ ) {
				const IndexedProbeContexts::IntRange idRange = idDataset.getOcclusionRange( idToleranceLevel );

				// is one of the ranges empty? if so, we don't need to check it at all
				if( idRange.first == idRange.second ) {
					continue;
				}

				// store the range for later
				overlappedRanges.push_back( std::make_pair( queryRange, idRange ) );
			}
		}

		using namespace Concurrency;
		boost::dynamic_bitset<> mergedProbesMatchedInner( indexedProbeContexts.size() ), mergedProbesMatchedOuter( idDataset.size() );
		combinable< int > numMatches;

		{
			combinable< boost::dynamic_bitset<> > probesMatchedInner, probesMatchedOuter;

			AUTO_TIMER_BLOCK( "matching" ) {
				parallel_for_each( overlappedRanges.begin(), overlappedRanges.end(),
					[&] ( const OverlappedRange &rangePair ) {
						probesMatchedInner.local().resize( indexedProbeContexts.size() );
						probesMatchedOuter.local().resize( idDataset.size() );

						matchSortedRanges(
							indexedProbeContexts.data,
							rangePair.first,
							probesMatchedInner.local(),

							idDataset.data,
							rangePair.second,
							probesMatchedOuter.local(),

							numMatches.local()
						);
					}
				);
			}
			AUTO_TIMER_BLOCK( "combining matches" ) {
				probesMatchedInner.combine_each(
					[&] ( const boost::dynamic_bitset<> &set ) {
						mergedProbesMatchedInner |= set;
					}
				);

				probesMatchedOuter.combine_each(
					[&] ( const boost::dynamic_bitset<> &set ) {
						mergedProbesMatchedOuter |= set;
					}
				);
			}
		}

		const auto numProbesMatchedInner = mergedProbesMatchedInner.count();
		const auto numProbesMatchedOuter = mergedProbesMatchedOuter.count();
		DetailedQueryResult detailedQueryResult( sceneModelIndex );
		detailedQueryResult.numMatches = numMatches.combine( std::plus<int>() );

		detailedQueryResult.probeMatchPercentage = float( numProbesMatchedOuter ) / idDataset.size();
		detailedQueryResult.queryMatchPercentage = float( numProbesMatchedInner ) / indexedProbeContexts.size();

		detailedQueryResult.score = detailedQueryResult.probeMatchPercentage * detailedQueryResult.queryMatchPercentage;

		return detailedQueryResult;
	}

	void matchSortedRanges(
		const DBProbeContexts &probeContextsInner,
		const IntRange &rangeInner,
		boost::dynamic_bitset<> &probesMatchedInner,

		const DBProbeContexts &probeContextsOuter,
		const IntRange &rangeOuter,
		boost::dynamic_bitset<> &probesMatchedOuter,

		int &numMatches
	) {
		const float squaredColorTolerance = probeContextTolerance.colorLabTolerance * probeContextTolerance.colorLabTolerance;
		// assert: the range is not empty

		// sort the ranges into two new vectors
		// idea: use a global scratch space to avoid recurring allocations?

		const int beginIndexInner = rangeInner.first;
		const int endIndexInner = rangeInner.second;
		int indexInner = beginIndexInner;

		const int beginIndexOuter = rangeOuter.first;
		const int endIndexOuter = rangeOuter.second;
		int indexOuter = beginIndexOuter;

		DBProbeContext probeContextOuter = probeContextsOuter[ indexOuter ];
		for( ; indexOuter < endIndexOuter - 1 ; indexOuter++ ) {
			const DBProbeContext nextContextOuter = probeContextsOuter[ indexOuter + 1 ];
			int nextIndexInner = indexInner;

			const float minDistance = probeContextOuter.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = probeContextOuter.distance + probeContextTolerance.distanceTolerance;
			const float minNextDistance = nextContextOuter.distance - probeContextTolerance.distanceTolerance;

			bool probeMatchedOuter = false;

			for( ; indexInner < endIndexInner ; indexInner++ ) {
				const DBProbeContext probeContextInner = probeContextsInner[ indexInner ];

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
					numMatches += probeContextOuter.weight * probeContextInner.weight;

					probesMatchedInner[ indexInner ] = true;
					probeMatchedOuter = true;
				}
			}

			if( probeMatchedOuter ) {
				probesMatchedOuter[ indexOuter ] = true;
			}

			probeContextOuter = nextContextOuter;
			indexInner = nextIndexInner;
		}

		// process the last pure probe
		{
			const float minDistance = probeContextOuter.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = probeContextOuter.distance + probeContextTolerance.distanceTolerance;

			bool probeMatchedOuter = false;

			for( ; indexInner < endIndexInner ; indexInner++ ) {
				const DBProbeContext probeContextInner = probeContextsInner[ indexInner ];

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
					numMatches += probeContextOuter.weight * probeContextInner.weight;

					probesMatchedInner[ indexInner ] = true;
					probeMatchedOuter = true;
				}
			}

			if( probeMatchedOuter ) {
				probesMatchedOuter[ indexOuter ] = true;
			}
		}
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
		this->indexedProbeContexts = IndexedProbeContexts( ProbeDatabase::transformContexts( rawProbeContexts ) );
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
	typedef IndexedProbeContexts::IntRange IntRange;
	typedef std::pair< IntRange, IntRange > OverlappedRange;

	DetailedQueryResult matchAgainst( int localSceneIndex, int sceneModelIndex ) {
		// TODO: rename idDataset to idDatabase? [9/26/2012 kirschan2]
		const auto &idDatasets = database.sampledModels[ localSceneIndex ];
		const IndexedProbeContexts &idDataset = idDatasets.getMergedInstances();

		if( idDataset.size() == 0 ) {
			return DetailedQueryResult( sceneModelIndex );
		}

		AUTO_TIMER_FOR_FUNCTION( boost::format( "id = %i, %i ref probes (%i query probes)" ) % database.modelIndexMapper.getSceneModelIndex( sceneModelIndex ) % idDataset.size() % indexedProbeContexts.size() );

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

		std::vector< OverlappedRange > overlappedRanges;
		overlappedRanges.reserve( OptixProgramInterface::numProbeSamples );
		for( int occulsionLevel = 0 ; occulsionLevel <= OptixProgramInterface::numProbeSamples ; occulsionLevel++ ) {
			const IndexedProbeContexts::IntRange queryRange = indexedProbeContexts.getOcclusionRange( occulsionLevel );

			if( queryRange.first == queryRange.second ) {
				continue;
			}

			const int leftToleranceLevel = std::max( 0, occulsionLevel - occlusionTolerance );
			const int rightToleranceLevel = std::min( occulsionLevel + occlusionTolerance, OptixProgramInterface::numProbeSamples );
			for( int idToleranceLevel = leftToleranceLevel ; idToleranceLevel <= rightToleranceLevel ; idToleranceLevel++ ) {
				const IndexedProbeContexts::IntRange idRange = idDataset.getOcclusionRange( idToleranceLevel );

				// is one of the ranges empty? if so, we don't need to check it at all
				if( idRange.first == idRange.second ) {
					continue;
				}

				// store the range for later
				overlappedRanges.push_back( std::make_pair( queryRange, idRange ) );
			}
		}

		using namespace Concurrency;
		std::vector< float > mergedProbesMatchedInner( indexedProbeContexts.size() ), mergedProbesMatchedOuter( idDataset.size() );
		combinable< int > numMatches;

		{
			combinable< std::vector< float > > probesMatchedInner, probesMatchedOuter;

			AUTO_TIMER_BLOCK( "matching" ) {
				parallel_for_each( overlappedRanges.begin(), overlappedRanges.end(),
					[&] ( const OverlappedRange &rangePair ) {
						probesMatchedInner.local().resize( indexedProbeContexts.size() );
						probesMatchedOuter.local().resize( idDataset.size() );

						matchSortedRanges(
							indexedProbeContexts.data,
							rangePair.first,
							probesMatchedInner.local(),

							idDatasets,
							rangePair.second,
							probesMatchedOuter.local(),

							numMatches.local()
						);
					}
				);
			}
			AUTO_TIMER_BLOCK( "combining matches" ) {
				probesMatchedInner.combine_each(
					[&] ( const std::vector< float > &matches ) {
						boost::transform( mergedProbesMatchedInner, matches, mergedProbesMatchedInner.begin(), std::max<float> );
					}
				);

				probesMatchedOuter.combine_each(
					[&] ( const std::vector< float > &matches ) {
						boost::transform( mergedProbesMatchedOuter, matches, mergedProbesMatchedOuter.begin(), std::max<float> );
					}
				);
			}
		}

		const float numProbesMatchedInner = boost::accumulate( mergedProbesMatchedInner, 0.0f, std::plus<float>() );
		const float numProbesMatchedOuter = boost::accumulate( mergedProbesMatchedOuter, 0.0f, std::plus<float>() );
		DetailedQueryResult detailedQueryResult( sceneModelIndex  );
		detailedQueryResult.numMatches = numMatches.combine( std::plus<int>() );

		const int numUncompressedModelProbeContexts = (int) idDatasets.getProbes().size() * (int) idDatasets.getInstances().size();
		const int numUncompressedQueryProbeContexts = indexedProbeContexts.size();
		detailedQueryResult.probeMatchPercentage = float( numProbesMatchedOuter ) / numUncompressedModelProbeContexts;
		detailedQueryResult.queryMatchPercentage = float( numProbesMatchedInner ) / numUncompressedQueryProbeContexts;

		detailedQueryResult.score = detailedQueryResult.probeMatchPercentage * detailedQueryResult.queryMatchPercentage;

		return detailedQueryResult;
	}

	static float getMatchScore( const DBProbe &query, const DBProbe &ref ) {
		const auto queryDirection = Eigen::map( query.direction );
		const auto refDirection = Eigen::map( ref.direction );

		//const float directionScore = (1.0 + queryDirection.dot( refDirection )) * 0.5;
		const float directionScore = queryDirection.dot( refDirection );
		if( directionScore <= 0.0 ) {
			return 0.0;
		}
		return directionScore;
		/*
		const auto queryPosition = Eigen::map( query.position );
		const auto refPosition = Eigen::map( ref.position );

		const float alpha = 0.001;
		const float beta = 0.00001;

		const Eigen::Vector3f delta = queryPosition - refPosition;
		const float deltaDot = delta.dot( refDirection );
		const float shiftScore =
			std::max(
				0.0,
				1.0 - (alpha * fabs( deltaDot ) + beta * sqrt(delta.squaredNorm() - deltaDot*deltaDot))
			);
		return directionScore * shiftScore;*/
	}

	void matchSortedRanges(
		// query
		const DBProbeContexts &probeContextsInner,
		const IntRange &rangeInner,
		std::vector< float > &probesMatchedInner,

		// ref
		const SampledModel &sampledModelOuter,
		const IntRange &rangeOuter,
		std::vector< float > &probesMatchedOuter,

		int &numMatches
	) {
		const float squaredColorTolerance = probeContextTolerance.colorLabTolerance * probeContextTolerance.colorLabTolerance;

		// assert: the range is not empty

		// sort the ranges into two new vectors
		// idea: use a global scratch space to avoid recurring allocations?

		const int beginIndexInner = rangeInner.first;
		const int endIndexInner = rangeInner.second;
		int indexInner = beginIndexInner;

		const int beginIndexOuter = rangeOuter.first;
		const int endIndexOuter = rangeOuter.second;
		int indexOuter = beginIndexOuter;

		DBProbeContext probeContextOuter = sampledModelOuter.getMergedInstances().getProbeContexts()[ indexOuter ];
		for( ; indexOuter < endIndexOuter - 1 ; indexOuter++ ) {
			const DBProbeContext nextContextOuter = sampledModelOuter.getMergedInstances().getProbeContexts()[ indexOuter + 1 ];
			int nextIndexInner = indexInner;

			const float minDistance = probeContextOuter.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = probeContextOuter.distance + probeContextTolerance.distanceTolerance;
			const float minNextDistance = nextContextOuter.distance - probeContextTolerance.distanceTolerance;

			float probeBestMatchOuter = 0.0f;

			for( ; indexInner < endIndexInner ; indexInner++ ) {
				const DBProbeContext probeContextInner = probeContextsInner[ indexInner ];

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
					numMatches++;

					const float matchScore = getMatchScore( probes[ probeContextInner.probeIndex ], sampledModelOuter.getProbes()[ probeContextOuter.probeIndex ] );

					probesMatchedInner[ indexInner ] = std::max( probeContextInner.weight * matchScore, probesMatchedInner[ indexInner ] );
					probeBestMatchOuter = std::max( probeBestMatchOuter, matchScore );
				}
			}

			if( probeBestMatchOuter > 0.0f ) {
				probesMatchedOuter[ indexOuter ] = std::max( probeContextOuter.weight * probeBestMatchOuter, probesMatchedOuter[ indexOuter ] );
			}

			probeContextOuter = nextContextOuter;
			indexInner = nextIndexInner;
		}

		// process the last pure probe
		{
			const float minDistance = probeContextOuter.distance - probeContextTolerance.distanceTolerance;
			const float maxDistance = probeContextOuter.distance + probeContextTolerance.distanceTolerance;

			float probeBestMatchOuter = 0.0f;

			for( ; indexInner < endIndexInner ; indexInner++ ) {
				const DBProbeContext probeContextInner = probeContextsInner[ indexInner ];

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
					numMatches++;

					const float matchScore = getMatchScore( probes[ probeContextInner.probeIndex ], sampledModelOuter.getProbes()[ probeContextOuter.probeIndex ] );
					probesMatchedInner[ indexInner ] = std::max( probeContextInner.weight * matchScore, probesMatchedInner[ indexInner ] );
					probeBestMatchOuter = matchScore;
				}
			}

			if( probeBestMatchOuter > 0.0f ) {
				probesMatchedOuter[ indexOuter ] = std::max( probeContextOuter.weight * probeBestMatchOuter, probesMatchedOuter[ indexOuter ] );
			}
		}
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