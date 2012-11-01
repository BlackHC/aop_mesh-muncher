#pragma once

#include <vector>
#include <Eigen/Eigen>
#include <boost/range/algorithm/sort.hpp>
#include <deque>
#include "boost/range/algorithm_ext/push_back.hpp"
#include "boost/range/algorithm_ext/erase.hpp"
#include "boost/range/algorithm/unique.hpp"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <map>

#include <boost/multi_array.hpp>

#include "boost/tuple/tuple.hpp"
#include "boost/tuple/tuple_comparison.hpp"

#include <logger.h>

#include "modelDatabase.h"

#include <serializer_fwd.h>

#pragma warning( push )
#pragma warning( once: 4244 )

namespace Neighborhood {
	struct NeighborhoodContext;
	struct SampledModel;
}

SERIALIZER_FWD_EXTERN_DECL( Neighborhood::NeighborhoodContext )
SERIALIZER_FWD_EXTERN_DECL( Neighborhood::SampledModel )

namespace Neighborhood {
	typedef int Id;
	const int INVALID_ID = -1;

	typedef std::vector< float > Distances;

	typedef std::pair< Id, float > IdDistancePair;

	typedef std::vector< IdDistancePair > RawIdDistances;
	typedef std::vector< Distances > DistancesById; // index with [id]!

	struct NeighborhoodContext {
		NeighborhoodContext() {}

		NeighborhoodContext( RawIdDistances &&rawDataset ) {
			if( rawDataset.empty() ) {
				return;
			}

			boost::sort( rawDataset );

			// rawData is sorted by id first
			const int numIds = rawDataset.back().first + 1;

			distancesById.resize( numIds );

			for( auto idDistancePair = rawDataset.begin() ; idDistancePair != rawDataset.end() ; ++idDistancePair ) {
				distancesById[ idDistancePair->first ].push_back( idDistancePair->second );
			}
		}

		const DistancesById &getDistancesById() const {
			return distancesById;
		}

		// never fails, returns an empty distances vector if the id is not found
		const Distances & getDistances( int id ) const {
			static const Distances emptyDistances;

			if( id < distancesById.size() ) {
				return distancesById[ id ];
			}
			else {
				return emptyDistances;
			}
		}

		int getNumIds() const {
			return distancesById.size();
		}

	private:
		DistancesById distancesById;

		SERIALIZER_FWD_FRIEND_EXTERN( Neighborhood::NeighborhoodContext );
	};

	inline float getEntropy( float certainty, float probability, float alpha = 0.1 ) {
		const float safeProbability = probability * (1.0 - alpha) + alpha * 0.5;
		const float positiveEntropy = -logf( safeProbability );
		const float negativeEntropy = -logf( 1.0 - safeProbability );

		const float expectedEntropy = certainty * positiveEntropy + (1.0 - certainty) * negativeEntropy;
		return expectedEntropy;
	}

	struct SampledModel {
		std::vector< NeighborhoodContext > instances;

		SampledModel() {}

		void addInstance( NeighborhoodContext &&sortedDataset ) {
			instances.emplace_back( std::move( sortedDataset ) );
		}

		SERIALIZER_FWD_FRIEND_EXTERN( Neighborhood::SampledModel );
	};

	struct NeighborhoodDatabaseV2 {
		ModelDatabase *modelDatabase;

		typedef std::pair< int, SampledModel > IdSampledModelPair;

		int numIds;
		int totalNumInstances;

		std::vector< IdSampledModelPair > sampledModelsById;

		NeighborhoodDatabaseV2() : numIds(), totalNumInstances() {}

		bool load( const std::string &filename );
		void store( const std::string &filename ) const;

		void clear() {
			numIds = 0;
			totalNumInstances = 0;
			sampledModelsById.clear();
		}

		int getNumSampledModels() const {
			return sampledModelsById.size();
		}

		const SampledModel &getSampledModel( Id id ) {
			return internal_getSampledModel( id );
		}

		SampledModel &internal_getSampledModel( Id id ) {
			for( auto idSampledModelPair = sampledModelsById.begin() ; idSampledModelPair != sampledModelsById.end() ; ++idSampledModelPair ) {
				if( idSampledModelPair->first == id ) {
					return idSampledModelPair->second;
				}
			}

			sampledModelsById.push_back( std::make_pair( id, SampledModel() ) );
			return sampledModelsById.back().second;
		}

		void addInstance( Id id, NeighborhoodContext &&sortedDataset ) {
			numIds = std::max( numIds, sortedDataset.getNumIds() );
			++totalNumInstances;
			internal_getSampledModel( id ).addInstance( std::move( sortedDataset ) );
		}

		struct Query {
			const NeighborhoodDatabaseV2 &database;
			const NeighborhoodContext queryDataset;

			const float queryTolerance;

			Query( const NeighborhoodDatabaseV2 &database, float queryTolerance, NeighborhoodContext &&sortedDataset )
				: database( database )
				, queryTolerance( queryTolerance )
				, queryDataset( std::move( sortedDataset ) )
			{
			}

			typedef std::pair< float, Id > Result;

			typedef std::vector< Result > Results;

			struct DefaultPolicy {
				static float getNeighborModelTolerance( Query *query, Id id ) {
					return query->database.modelDatabase->informationById[id].diagonalLength * 0.5f;
				}

				static float getNeighborModelWeight( Query *query, Id id ) {
					return query->database.modelDatabase->informationById[id].diagonalLength;
				}

				/*static float getDistanceWeight( float neighborModelTolerance, float distance ) {
					return neighborModelTolerance / distance;
				}*/

				static float getDistanceToleranceScale( float distance ) {
					return 1.0f + (distance - 1.0f) * 0.5f;
				}
			};

			// this structure holds information a distance that has not been matched
			struct UnmatchedDistance {
				float distance;
				int candidateModelIndex;
				int globalInstanceIndex;

				UnmatchedDistance( float distance, int candidateModelIndex, int globalInstanceIndex ) :
					distance( distance ),
					candidateModelIndex( candidateModelIndex ),
					globalInstanceIndex( globalInstanceIndex )
				{
				}

				static bool less_by_distance( const UnmatchedDistance &a, const UnmatchedDistance &b ) {
					return a.distance < b.distance;
				}

				static bool less_by_globalInstanceIndex( const UnmatchedDistance &a, const UnmatchedDistance &b ) {
					return a.globalInstanceIndex < b.globalInstanceIndex;
				}
			};
			typedef std::vector< UnmatchedDistance > UnmatchedDistances;

			template< class Policy >
			UnmatchedDistances matchDistances(
				Id sceneNeighborModelId,
				int &numMatchedDistances,
				std::vector<float> &neighborModelCandidateScores,
				std::vector<float> &totalImportanceWeights
			) {
				const int numSampledModels = database.sampledModelsById.size();

				// get the query volume's distances for the current neighbor model
				const Distances &queryDistances = queryDataset.getDistances( sceneNeighborModelId );
				const int numQueryDistances = queryDistances.size();

				const float neighborModelTolerance = Policy::getNeighborModelTolerance( this, sceneNeighborModelId );

				// counts all matches for a certain query distance and candidateModel
				boost::multi_array<int, 2> candidateMatchedDistancesCounters( boost::extents[numSampledModels][numQueryDistances] );
				// counts all matches for a certain query distance
				std::vector<int> matchedDistancesCounters( numQueryDistances );

				UnmatchedDistances unmatchedDistances;

				int globalInstanceIndex = 0;
				for( int candidateModelIndex = 0 ; candidateModelIndex < numSampledModels ; ++candidateModelIndex ) {
					const SampledModel &candidateModel = database.sampledModelsById[ candidateModelIndex ].second;

					for( int instanceIndex = 0 ; instanceIndex < candidateModel.instances.size() ; ++instanceIndex, ++globalInstanceIndex ) {
						const auto &instance = candidateModel.instances[ instanceIndex ];
						const Distances &instanceDistances = instance.getDistances( sceneNeighborModelId );

						auto instanceDistance = instanceDistances.begin();
						for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryDistances ; ++queryDistanceIndex ) {
							const float queryDistance = queryDistances[ queryDistanceIndex ];
							const float queryDistanceToleranceScale = Policy::getDistanceToleranceScale( queryDistance );

							const float tolerance = queryDistanceToleranceScale * neighborModelTolerance + queryTolerance;
							const float beginQueryDistanceInterval = queryDistance - tolerance;
							const float endQueryDistanceInterval = queryDistance + tolerance;

							for( ; instanceDistance != instanceDistances.end() && *instanceDistance < beginQueryDistanceInterval ; ++instanceDistance ) {
								// there is no query distance that can match this instance distance, so its a mismatch
								unmatchedDistances.emplace_back( UnmatchedDistance( *instanceDistance, candidateModelIndex, globalInstanceIndex ) );
							}

							if( instanceDistance == instanceDistances.end() ) {
								break;
							}

							// can this instance distance be matched by this query?
							if( *instanceDistance <= endQueryDistanceInterval ) {
								// match
								++matchedDistancesCounters[ queryDistanceIndex ];
								++candidateMatchedDistancesCounters[ candidateModelIndex ][ queryDistanceIndex ];
								++numMatchedDistances;

								// we're done with this instance distance
								++instanceDistance;
							}
						}

						// the remaining instance elements are mismatches, too
						for( ; instanceDistance != instanceDistances.end() ; ++instanceDistance ) {
							unmatchedDistances.emplace_back( UnmatchedDistance( *instanceDistance, candidateModelIndex, globalInstanceIndex ) );
						}
					}
				}

				const int numUnmatchedDistances = unmatchedDistances.size();

				const int totalNumDistances = numMatchedDistances + numUnmatchedDistances + numQueryDistances;

				//const int numTotalInstances = globalInstanceIndex;
				if( database.totalNumInstances > 0 && numQueryDistances > 0 ) {
					// calculate the match probabilities
					for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryDistances ; ++queryDistanceIndex ) {
						if( matchedDistancesCounters[ queryDistanceIndex ] == 0 ) {
							continue;
						}

						// binWeight = #bin distances / #all distances
						// = P( D | N )
						const float binWeight = float(matchedDistancesCounters[queryDistanceIndex] + 1) / totalNumDistances;
						
						for( int candidateModelIndex = 0 ; candidateModelIndex < numSampledModels ; ++candidateModelIndex ) {
							const SampledModel &candidateModel = database.sampledModelsById[ candidateModelIndex ].second;
							const int numCandidateModelInstances = candidateModel.instances.size();
							if( numCandidateModelInstances == 0 ) {
								continue;
							}

							// = P( M | N, D )
							const float candidateModelWeight = float( numCandidateModelInstances ) / database.totalNumInstances;
							// = P( V = 1 | M, N, D )
							const float conditionalProbability = float( candidateMatchedDistancesCounters[ candidateModelIndex ][ queryDistanceIndex ] ) / numCandidateModelInstances;
							const float certainty = conditionalProbability;

							const float averageProbability = float(matchedDistancesCounters[queryDistanceIndex] + 1) / database.totalNumInstances;
							const float importanceWeight = getEntropy( certainty, averageProbability );
							neighborModelCandidateScores[ candidateModelIndex ] += candidateModelWeight * conditionalProbability * importanceWeight;
							totalImportanceWeights[ candidateModelIndex ] += importanceWeight;
						}
					}
				}

				log( boost::format( "%i: %i + %i" ) % sceneNeighborModelId % numQueryDistances % numUnmatchedDistances );

				return unmatchedDistances;
			}

			template< class Policy >
			void processUnmatchedDistances(
				Id sceneNeighborModelId,
				UnmatchedDistances &&unmatchedDistances,
				const int numMatchedDistances,
				std::vector<float> &neighborModelCandidateScores,
				std::vector<float> &totalImportanceWeights
			) {
				const int numUnmatchedDistances = unmatchedDistances.size();

				// initialize short-hand references
				const int numSampledModels = database.sampledModelsById.size();
				const float neighborModelTolerance = Policy::getNeighborModelTolerance( this, sceneNeighborModelId );

				const auto &queryData = queryDataset.getDistances( sceneNeighborModelId );
				const int numQueryDistances = queryData.size();

				const int numTotalMatches = numMatchedDistances + numQueryDistances + numUnmatchedDistances;

				// we need another vector to hold the mismatches we cant process
				UnmatchedDistances deferredUnmatchedDistances;
				deferredUnmatchedDistances.reserve( unmatchedDistances.size() );

				// second pass: process mismatches
				while( !unmatchedDistances.empty() ) {
					// sort the mismatches by distance first
					boost::sort( unmatchedDistances, UnmatchedDistance::less_by_distance );

					// process mismatches implicitly distance bin by distance bin
					for( auto unmatchedDistance = unmatchedDistances.begin() ; unmatchedDistance != unmatchedDistances.end() ; ) {
						const auto binBegin = unmatchedDistance;
						{
							const float beginDistance = binBegin->distance;
							const float toleranceScale = Policy::getDistanceToleranceScale( binBegin->distance );
							// this isn't exactly right but I dont want to write an inverter just now
							// TODO: write an inverter [10/11/2012 kirschan2]
							const float endDistance = beginDistance + 2 * neighborModelTolerance * toleranceScale + 2 * queryTolerance;
							do {
								++unmatchedDistance;
							} while(
									unmatchedDistance != unmatchedDistances.end()
								&&
									unmatchedDistance->distance <= endDistance
							);
						}
						const auto binEnd = unmatchedDistance;

						// sort by globalInstanceIndex and then entry (globalInstanceIndex is already sorted by entry implicitly)
						std::sort( binBegin, binEnd, UnmatchedDistance::less_by_globalInstanceIndex );

						// count the instances in this bin
						int numInstancesInBin = 0;
						for( auto binElement = binBegin ; binElement != binEnd ; ) {
							const int globalInstanceIndex = binElement->globalInstanceIndex;
							++binElement;
							++numInstancesInBin;

							// skip over additional mismatches from globalInstanceIndex in this bin
							// and add them to the list of deferred mismatches because we cant process them this round
							while(
									binElement != binEnd
								&&
									binElement->globalInstanceIndex == globalInstanceIndex
							) {
								deferredUnmatchedDistances.push_back( *binElement );
								++binElement;
							}
						}

						// count the entry elements in this bin and calculate the probability of the mismatch
						for( auto binElement = binBegin ; binElement != binEnd ; ) {
							const int candidateModelIndex = binElement->candidateModelIndex;

							// count the models in this bin
							int numCandidateModelInstancesInBin = 0;
							do {
								const int globalInstanceIndex = binElement->globalInstanceIndex;
								do {
									binElement++;
								}
								while(
										binElement != binEnd
									&&
										binElement->globalInstanceIndex == globalInstanceIndex
								);

								numCandidateModelInstancesInBin++;
							}
							while(
									binElement != binEnd
								&&
									binElement->candidateModelIndex == candidateModelIndex
							);

							// calculate the conditional probability that we have a mismatch
							const SampledModel &candidateModel = database.sampledModelsById[ candidateModelIndex ].second;
							const int numCandidateModelInstances = candidateModel.instances.size();

							// TODO: verify that numCandidateModelInstances == 0 is impossible because there would be no mismatch otherwise.. [10/11/2012 kirschan2]
							/*if( numCandidateModelInstances == 0 ) {
								continue;
							}*/
							
							//const float candidateModelWeight = numCandidateModelInstances / database.totalNumInstances;										
							// candidateProbability = 0 here, because there is no match
							
							const float certainty = float( numCandidateModelInstancesInBin ) / numCandidateModelInstances;
							const float averageProbability = float( numInstancesInBin ) / database.totalNumInstances;

							const float importanceWeight = getEntropy( certainty, averageProbability );

							totalImportanceWeights[ candidateModelIndex ] += importanceWeight;
						}
					}

					// use the left over mismatches for the next round until we're done with everything
					unmatchedDistances.swap( deferredUnmatchedDistances );
					deferredUnmatchedDistances.clear();
				}
			}

			template< class Policy >
			std::vector<float> matchAgainstNeighborModel( Id sceneNeighborModelId ) {
				const int numSampledModels = database.sampledModelsById.size();

				// scores for the current neighborModelIndex
				std::vector<float> neighborModelCandidateScores( numSampledModels );
				std::vector<float> totalImportanceWeights( numSampledModels );

				int numMatchedDistances = 0;

				// first phase: matches + query mismatches
				std::vector< UnmatchedDistance > unmatchedDistances = matchDistances<Policy>( sceneNeighborModelId, numMatchedDistances, neighborModelCandidateScores, totalImportanceWeights );

				// this is the number of merged mismatched distances
				processUnmatchedDistances<Policy>(
					sceneNeighborModelId,
					std::move( unmatchedDistances ),
					numMatchedDistances,
					neighborModelCandidateScores,
					totalImportanceWeights
				);

				for( int candidateModelIndex = 0 ; candidateModelIndex < numSampledModels ; ++candidateModelIndex ) {
					const float totalImportanceWeight = totalImportanceWeights[ candidateModelIndex ];
					if( totalImportanceWeight > 0.0f ) {
						neighborModelCandidateScores[ candidateModelIndex ] /= totalImportanceWeight;
					}
				}

				return neighborModelCandidateScores;
			}

			template< class Policy >
			Results executeWithPolicy() {
				const int numSampledModels = database.getNumSampledModels();
				// total score of all candidates
				std::vector<float> totalCandidateScores( numSampledModels );

				float totalScore = 0;

				// we iterate over all model ids and compare the query distances against all sampled models---one neighbor model at a time
				for( int sceneNeighborModelId = 0 ; sceneNeighborModelId < database.numIds ; ++sceneNeighborModelId ) {
					// = P( D | N )
					const float neighborModelWeight = Policy::getNeighborModelWeight( this, sceneNeighborModelId );

					if( neighborModelWeight == 0.0f ) {
						continue;
					}

					const std::vector<float> neighborModelCandidateScores = matchAgainstNeighborModel<Policy>( sceneNeighborModelId );

					// update the total score
					for( int candidateModelIndex = 0 ; candidateModelIndex < numSampledModels ; ++candidateModelIndex ) {
						const float weightedScore = neighborModelCandidateScores[ candidateModelIndex ] * neighborModelWeight;
						totalCandidateScores[ candidateModelIndex ] += weightedScore;
						totalScore += weightedScore;
					}
				}

				// compute the final score and store it in our results data structure
				if( totalScore > 0.0f )	{
					Results results;
					for( int candidateModelIndex = 0 ; candidateModelIndex < numSampledModels ; ++candidateModelIndex ) {
						const auto sceneCandidateModelId = database.sampledModelsById[ candidateModelIndex ].first;

						const float score = totalCandidateScores[ candidateModelIndex ] / totalScore;
						// TOOD: hack to remove invisible objects [10/11/2012 kirschan2]
						if( database.modelDatabase->informationById[ sceneCandidateModelId ].diagonalLength > 0 ) {
							results.push_back( Result( score, sceneCandidateModelId ) );
						}
					}
					boost::sort( results, std::greater< Result >() );
					return results;
				}
				else {
					return Results();
				}
			}

			Results execute() {
				return executeWithPolicy< DefaultPolicy >();
			}
		};
	};
}

#pragma warning( pop )