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

#include <boost/dynamic_bitset.hpp>

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

	typedef std::pair< float, Id > Result;
	typedef std::vector< Result > Results;

	RawIdDistances loadRawIdDistances( const std::string &filename );
	void storeRawIdDistances( const std::string &filename, RawIdDistances &rawIdDistances );

	Results loadResults( const std::string &filename );
	void storeResults( const std::string &filename, const Results &results );

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

#if 0
	inline float getMessageLength( float certainty, float probability, float alpha = 0.1f ) {
		const float safeProbability = probability * (1.0f - alpha) + alpha * 0.5f;
		const float positiveEntropy = -logf( safeProbability );
		const float negativeEntropy = -logf( 1.0 - safeProbability );

		const float expectedEntropy = certainty * positiveEntropy + (1.0 - certainty) * negativeEntropy;
		return expectedEntropy;
	}
#endif

	inline float combinePositiveAndNegativeScores( float score, float maxNegativeScore, float maxPositiveScore ) {
		float threshold = 0.2;
		if( score >= 0.0f ) {
			return score / maxPositiveScore * (1.0 - threshold) + threshold;
		}
		else {
			return (1.0 + score / maxNegativeScore) * threshold;
		}
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

		NeighborhoodDatabaseV2() : numIds(), totalNumInstances(), modelDatabase() {}

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

		int getTotalNumInstances() const {
			return totalNumInstances;
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

			// this structure holds information about a distance that has not been matched
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

			struct SharedPolicy {
				static float getNeighborModelTolerance( Query *query, Id id ) {
					return query->database.modelDatabase->informationById[id].diagonalLength * 0.5f;
				}

				static float getNeighborModelWeight( Query *query, Id id ) {
					return logf( query->database.modelDatabase->informationById[id].diagonalLength + 3 );
				}

				/*static float getDistanceWeight( float neighborModelTolerance, float distance ) {
					return neighborModelTolerance / distance;
				}*/

				static float getDistanceToleranceScale( float distance ) {
					return 1.0f + (distance - 1.0f) * 0.5f;
				}
			};

			struct SimpleMatchedDistances {
				// counts all matches for a certain query distance and candidateModel
				std::vector<int> matchedQueryDistancesByGlobalInstance;
				// counts all matches for a certain query distance
				int numQueryDistances;

				SimpleMatchedDistances( int totalNumInstances, int numQueryDistances )
					: matchedQueryDistancesByGlobalInstance( totalNumInstances )
					, numQueryDistances( numQueryDistances )
				{}

				void match( int globalInstanceIndex, int queryDistanceIndex ) {
					matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ] += 1;
				}

				int getTotalNumInstances() {
					return matchedQueryDistancesByGlobalInstance.size();
				}

				int getNumQueryDistances() {
					return numQueryDistances;
				}
			};

			struct FullMatchedDistances {
				// counts all matches for a certain query distance and candidateModel
				// by [ globalInstanceIndex ][ queryDistanceIndex ]
				std::vector< boost::dynamic_bitset<> > matchedQueryDistancesByGlobalInstance;
				// counts all matches for a certain query distance
				std::vector<int> matchedQueryDistanceCounters;

				FullMatchedDistances( int totalNumInstances, int numQueryDistances )
					: matchedQueryDistancesByGlobalInstance( totalNumInstances, boost::dynamic_bitset<>( numQueryDistances ) )
					, matchedQueryDistanceCounters( numQueryDistances )
				{}

				void match( int globalInstanceIndex, int queryDistanceIndex ) {
					matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ][ queryDistanceIndex ] = true;
					++matchedQueryDistanceCounters[ queryDistanceIndex ];
				}

				int getTotalNumInstances() {
					return matchedQueryDistancesByGlobalInstance.size();
				}

				int getNumQueryDistances() {
					return matchedQueryDistanceCounters.size();
				}
			};

			struct UniformWeightPolicy : SharedPolicy {
				typedef SimpleMatchedDistances MatchedDistances;

				struct Scores {
					std::vector<float> candidateInstanceScores;
					float totalImportanceWeight;

					float scoreOffset;

					Scores( int totalNumInstances )
						: candidateInstanceScores( totalNumInstances )
						, totalImportanceWeight()
						, scoreOffset()
					{}

					bool isEmpty() {
						return totalImportanceWeight == 0.0f;
					}

					void integrateNeighborModelCandidateScores( float neighborModelWeight, const Scores &neighborModelCandidateScores ) {
						const int totalNumInstances = candidateInstanceScores.size();
						// update the total score
						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; globalInstanceIndex++ ) {
							candidateInstanceScores[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									(neighborModelCandidateScores.scoreOffset + neighborModelCandidateScores.candidateInstanceScores[ globalInstanceIndex ])
							;
						}
						totalImportanceWeight += neighborModelWeight * neighborModelCandidateScores.totalImportanceWeight;
					}

					void integrateMatchedDistances( MatchedDistances &matchedDistances ) {
						const int numQueryDistances = matchedDistances.getNumQueryDistances();
						const int totalNumInstances = matchedDistances.getTotalNumInstances();

						totalImportanceWeight += numQueryDistances;

						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; ++globalInstanceIndex ) {
							candidateInstanceScores[ globalInstanceIndex ] += matchedDistances.matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ];
						}
					}

					void integrateCorrelatedUnmatchedDistances( std::vector< int > &&correlatedGlobalInstanceIndices ) {
						totalImportanceWeight += 1;
						scoreOffset += 1;

						for(
							auto correlatedGlobalInstanceIndex = correlatedGlobalInstanceIndices.begin() ;
							correlatedGlobalInstanceIndex != correlatedGlobalInstanceIndices.end() ;
							++correlatedGlobalInstanceIndex
						) {
							candidateInstanceScores[ *correlatedGlobalInstanceIndex ] -= 1;
						}
					}

					float getInstanceScore( int globalInstanceIndex ) const {
						return (candidateInstanceScores[ globalInstanceIndex ] + scoreOffset) / totalImportanceWeight;
					}
				};
			};

			struct ImportanceWeightPolicy : SharedPolicy {
				typedef FullMatchedDistances MatchedDistances;

				struct Scores {
					std::vector<float> candidateInstanceScores;
					std::vector<float> candidateInstanceImportanceWeights;

					float offset;

					Scores( int totalNumInstances )
						: candidateInstanceScores( totalNumInstances )
						, candidateInstanceImportanceWeights( totalNumInstances )
						, offset()
					{}

					bool isEmpty() {
						return false;
					}

					void integrateNeighborModelCandidateScores( float neighborModelWeight, const Scores &neighborModelCandidateScores ) {
						const int totalNumInstances = candidateInstanceScores.size();
						// update the total score
						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; globalInstanceIndex++ ) {
							candidateInstanceScores[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									(neighborModelCandidateScores.offset + neighborModelCandidateScores.candidateInstanceScores[ globalInstanceIndex ])
							;
							candidateInstanceImportanceWeights[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									(neighborModelCandidateScores.offset + neighborModelCandidateScores.candidateInstanceImportanceWeights[ globalInstanceIndex ])
							;
						}
					}

					void integrateMatchedDistances( MatchedDistances &matchedDistances ) {
						const int numQueryDistances = matchedDistances.getNumQueryDistances();
						const int totalNumInstances = matchedDistances.getTotalNumInstances();

						std::vector<float> matchImportanceWeights( numQueryDistances );
						std::vector<float> mismatchImportanceWeights( numQueryDistances );

						for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryDistances ; ++queryDistanceIndex ) {
							const float frequency = (matchedDistances.matchedQueryDistanceCounters[ queryDistanceIndex ] + 1.0f) / (totalNumInstances + 2.0f);

							const float positiveMessageLength = getMessageLength( frequency );
							const float negativeMessageLength = getMessageLength( 1.0f - frequency );

							// match means: query and instance have a matched distance
							// mismatch means: query has a matched distance, but instance has none
							matchImportanceWeights[ queryDistanceIndex ] = positiveMessageLength * 2.0f;
							mismatchImportanceWeights[ queryDistanceIndex ] = positiveMessageLength + negativeMessageLength;
						}

						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; ++globalInstanceIndex ) {
							for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryDistances ; ++queryDistanceIndex ) {
								if( matchedDistances.matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ][ queryDistanceIndex ] ) {
									// this is a match
									const float importanceWeight = matchImportanceWeights[ queryDistanceIndex ];
									candidateInstanceScores[ globalInstanceIndex ] += importanceWeight;
									candidateInstanceImportanceWeights[ globalInstanceIndex ] += importanceWeight;
								}
								else {
									// this is a mismatch
									candidateInstanceImportanceWeights[ globalInstanceIndex ] += mismatchImportanceWeights[ queryDistanceIndex ];
								}
							}
						}
					}

					void integrateCorrelatedUnmatchedDistances( std::vector< int > &&correlatedGlobalInstanceIndices ) {
						const int totalNumInstances = candidateInstanceScores.size();

						const int numInstances = correlatedGlobalInstanceIndices.size();

						const float negativeFrequency = (numInstances + 1.0f) / (totalNumInstances + 2.0f);

						const float positiveMessageLength = getMessageLength( 1.0f - negativeFrequency );
						const float negativeMessageLength = getMessageLength( negativeFrequency );

						// match means: the query and the instance have no distance
						const float matchImportanceWeight = negativeMessageLength * 2.0f;
						// mismatch means: the query has no distance, but the instance has
						const float mismatchImportanceWeight = negativeMessageLength + positiveMessageLength;

						// we process all unmatched instances now (assuming frequency <0.5...!)
						// everybody gets a matchImportanceWeight offset
						offset += matchImportanceWeight;

						// now we correct the scores and weights for the mismatches
						const float correctionScore = -matchImportanceWeight;
						const float correctionWeight = mismatchImportanceWeight - matchImportanceWeight;
						for(
							auto correlatedGlobalInstanceIndex = correlatedGlobalInstanceIndices.begin() ;
							correlatedGlobalInstanceIndex != correlatedGlobalInstanceIndices.end() ;
							++correlatedGlobalInstanceIndex
						) {
							candidateInstanceScores[ *correlatedGlobalInstanceIndex ] += correctionScore;
							candidateInstanceImportanceWeights[ *correlatedGlobalInstanceIndex ] += correctionWeight;
						}
					}

					float getInstanceScore( int globalInstanceIndex ) const {
						const float finalScore = candidateInstanceScores[ globalInstanceIndex ] + offset;
						const float finalWeight = candidateInstanceImportanceWeights[ globalInstanceIndex ] + offset;
						if( finalWeight != 0.0 ) {
							return finalScore / finalWeight;
						}
						return 0.0f;
					}
				};
			};

			struct CrazyImportanceWeightPolicy : SharedPolicy {
				typedef FullMatchedDistances MatchedDistances;

				struct Scores {
					std::vector<float> candidateInstanceScores;
					std::vector<float> candidateInstanceImportanceWeights;

					float offset;

					Scores( int totalNumInstances )
						: candidateInstanceScores( totalNumInstances )
						, candidateInstanceImportanceWeights( totalNumInstances )
						, offset()
					{}

					bool isEmpty() {
						return false;
					}

					void integrateNeighborModelCandidateScores( float neighborModelWeight, const Scores &neighborModelCandidateScores ) {
						const int totalNumInstances = candidateInstanceScores.size();
						// update the total score
						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; globalInstanceIndex++ ) {
							candidateInstanceScores[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									(neighborModelCandidateScores.offset + neighborModelCandidateScores.candidateInstanceScores[ globalInstanceIndex ])
							;
							candidateInstanceImportanceWeights[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									(neighborModelCandidateScores.offset + neighborModelCandidateScores.candidateInstanceImportanceWeights[ globalInstanceIndex ])
							;
						}
					}

					void integrateMatchedDistances( MatchedDistances &matchedDistances ) {
						const int numQueryDistances = matchedDistances.getNumQueryDistances();
						const int totalNumInstances = matchedDistances.getTotalNumInstances();

						std::vector<float> matchImportanceWeights( numQueryDistances );
						std::vector<float> mismatchImportanceWeights( numQueryDistances );

						for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryDistances ; ++queryDistanceIndex ) {
							const float frequency = (matchedDistances.matchedQueryDistanceCounters[ queryDistanceIndex ] + 0.0f) / (totalNumInstances + 0.0f);

							const float positiveMessageLength = getMessageLength( frequency );
							const float negativeMessageLength = getMessageLength( 1.0f - frequency );

							// match means: query and instance have a matched distance
							// mismatch means: query has a matched distance, but instance has none
							matchImportanceWeights[ queryDistanceIndex ] = positiveMessageLength * 2.0f;
							mismatchImportanceWeights[ queryDistanceIndex ] = positiveMessageLength + negativeMessageLength;
						}

						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; ++globalInstanceIndex ) {
							for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryDistances ; ++queryDistanceIndex ) {
								if( matchedDistances.matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ][ queryDistanceIndex ] ) {
									// this is a match
									const float importanceWeight = matchImportanceWeights[ queryDistanceIndex ];
									candidateInstanceScores[ globalInstanceIndex ] += importanceWeight;
									candidateInstanceImportanceWeights[ globalInstanceIndex ] += importanceWeight;
								}
								else {
									// this is a mismatch
									candidateInstanceImportanceWeights[ globalInstanceIndex ] += mismatchImportanceWeights[ queryDistanceIndex ];
								}
							}
						}
					}

					void integrateCorrelatedUnmatchedDistances( std::vector< int > &&correlatedGlobalInstanceIndices ) {
						const int totalNumInstances = candidateInstanceScores.size();

						const int numInstances = correlatedGlobalInstanceIndices.size();

						const float negativeFrequency = (numInstances + 0.0f) / (totalNumInstances + 0.0f);

						const float positiveMessageLength = getMessageLength( 1.0f - negativeFrequency );
						const float negativeMessageLength = getMessageLength( negativeFrequency );

						// match means: the query and the instance have no distance
						const float matchImportanceWeight = negativeMessageLength * 2.0f;
						// mismatch means: the query has no distance, but the instance has
						const float mismatchImportanceWeight = negativeMessageLength + positiveMessageLength;

						// we process all unmatched instances now (assuming frequency <0.5...!)
						// everybody gets a matchImportanceWeight offset
						offset += matchImportanceWeight;

						// now we correct the scores and weights for the mismatches
						const float correctionScore = -matchImportanceWeight;
						const float correctionWeight = mismatchImportanceWeight - matchImportanceWeight;
						for(
							auto correlatedGlobalInstanceIndex = correlatedGlobalInstanceIndices.begin() ;
							correlatedGlobalInstanceIndex != correlatedGlobalInstanceIndices.end() ;
							++correlatedGlobalInstanceIndex
						) {
							candidateInstanceScores[ *correlatedGlobalInstanceIndex ] += correctionScore;
							candidateInstanceImportanceWeights[ *correlatedGlobalInstanceIndex ] += correctionWeight;
						}
					}

					float getInstanceScore( int globalInstanceIndex ) const {
						const float finalScore = candidateInstanceScores[ globalInstanceIndex ] + offset;
						const float finalWeight = candidateInstanceImportanceWeights[ globalInstanceIndex ] + offset;
						if( finalWeight != 0.0 ) {
							return finalScore / finalWeight;
						}
						return 0.0f;
					}
				};
			};

			struct JaccardIndexPolicy : SharedPolicy {
				typedef SimpleMatchedDistances MatchedDistances;

				struct Scores {
					std::vector<float> candidateInstanceScores;
					std::vector<float> candidateInstanceImportanceWeights;

					float weightOffset;

					Scores( int totalNumInstances )
						: candidateInstanceScores( totalNumInstances )
						, candidateInstanceImportanceWeights( totalNumInstances )
						, weightOffset()
					{}

					bool isEmpty() {
						return false;
					}

					void integrateNeighborModelCandidateScores( float neighborModelWeight, const Scores &neighborModelCandidateScores ) {
						const int totalNumInstances = candidateInstanceScores.size();
						// update the total score
						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; globalInstanceIndex++ ) {
							candidateInstanceScores[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									neighborModelCandidateScores.candidateInstanceScores[ globalInstanceIndex ]
							;
							candidateInstanceImportanceWeights[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									(neighborModelCandidateScores.weightOffset + neighborModelCandidateScores.candidateInstanceImportanceWeights[ globalInstanceIndex ])
							;
						}
					}

					void integrateMatchedDistances( MatchedDistances &matchedDistances ) {
						const int numQueryDistances = matchedDistances.getNumQueryDistances();
						const int totalNumInstances = matchedDistances.getTotalNumInstances();

						// numQueryDistances = M11 + M10
						// matchCount = M11

						weightOffset += numQueryDistances;

						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; ++globalInstanceIndex ) {
							const int matchCount = matchedDistances.matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ];
							candidateInstanceScores[ globalInstanceIndex ] += matchCount;
						}
					}

					void integrateCorrelatedUnmatchedDistances( std::vector< int > &&correlatedGlobalInstanceIndices ) {
						// M01 += 1
						for(
							auto correlatedGlobalInstanceIndex = correlatedGlobalInstanceIndices.begin() ;
							correlatedGlobalInstanceIndex != correlatedGlobalInstanceIndices.end() ;
							++correlatedGlobalInstanceIndex
						) {
							candidateInstanceImportanceWeights[ *correlatedGlobalInstanceIndex ] += 1.0f;
						}
					}

					float getInstanceScore( int globalInstanceIndex ) const {
						const float score = candidateInstanceScores[ globalInstanceIndex ];
						const float weight = candidateInstanceImportanceWeights[ globalInstanceIndex ] + weightOffset;
						if( weight != 0.0f ) {
							return score / weight;
						}
						else {
							return 0.0f;
						}
					}
				};
			};

// Im unsure about these, so Im not using them for now
#if 0
			struct UniformScorePolicy : SharedPolicy {
				struct MatchedDistances {
					// counts all matches for a certain query distance and candidateModel
					std::vector<int> matchedQueryDistancesByGlobalInstance;
					int numQueryDistances;

					MatchedDistances( int totalNumInstances, int numQueryDistances )
						: matchedQueryDistancesByGlobalInstance( totalNumInstances )
						, numQueryDistances( numQueryDistances )
					{}

					void match( int globalInstanceIndex, int queryDistanceIndex ) {
						++matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ];
					}

					int getTotalNumInstances() {
						return matchedQueryDistancesByGlobalInstance.size();
					}

					int getNumQueryDistances() {
						return numQueryDistances;
					}
				};

				struct Scores {
					std::vector<float> candidateInstanceScores;
					float weightedTotalNumQueryDistances;
					float weightedTotalNumUnmatchedDistances;

					Scores( int totalNumInstances )
						: candidateInstanceScores( totalNumInstances )
						, weightedTotalNumQueryDistances()
						, weightedTotalNumUnmatchedDistances()
					{}

					bool isEmpty() {
						return
								weightedTotalNumQueryDistances == 0.0f
							&&
								weightedTotalNumUnmatchedDistances == 0.0f
						;
					}

					void integrateNeighborModelCandidateScores( float neighborModelWeight, const Scores &neighborModelCandidateScores ) {
						const int totalNumInstances = candidateInstanceScores.size();
						// update the total score
						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; globalInstanceIndex++ ) {
							candidateInstanceScores[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									neighborModelCandidateScores.candidateInstanceScores[ globalInstanceIndex ]
							;
						}
						weightedTotalNumQueryDistances += neighborModelWeight * neighborModelCandidateScores.weightedTotalNumQueryDistances;
						weightedTotalNumUnmatchedDistances += neighborModelWeight * neighborModelCandidateScores.weightedTotalNumUnmatchedDistances;
					}

					void integrateMatchedDistances( MatchedDistances &matchedDistances ) {
						const int numQueryDistances = matchedDistances.getNumQueryDistances();
						weightedTotalNumQueryDistances += numQueryDistances;

						const int totalNumInstances = matchedDistances.getTotalNumInstances();
						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; ++globalInstanceIndex ) {
							candidateInstanceScores[ globalInstanceIndex ] += matchedDistances.matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ];
						}
					}

					void integrateCorrelatedUnmatchedDistances( std::vector< int > &&correlatedGlobalInstanceIndices ) {
						weightedTotalNumUnmatchedDistances += 1;

						for(
							auto correlatedGlobalInstanceIndex = correlatedGlobalInstanceIndices.begin() ;
							correlatedGlobalInstanceIndex != correlatedGlobalInstanceIndices.end() ;
							++correlatedGlobalInstanceIndex
						) {
							candidateInstanceScores[ *correlatedGlobalInstanceIndex ] -= 1;
						}
					}

					float getInstanceScore( int globalInstanceIndex ) const {
						// alternative: use two ranges: 0.1..1.0 for positive scores and 0.0 to 0.1 for negative scores
						const float candidateInstanceScore = candidateInstanceScores[ globalInstanceIndex ];
						return combinePositiveAndNegativeScores( candidateInstanceScore, weightedTotalNumUnmatchedDistances, weightedTotalNumQueryDistances );
					}
				};
			};

			struct ImportanceScorePolicy : SharedPolicy {
				struct MatchedDistances {
					// counts all matches for a certain query distance and candidateModel
					std::vector< boost::dynamic_bitset<> > matchedQueryDistancesByGlobalInstance;
					// counts all matches for a certain query distance
					std::vector<int> matchedQueryDistanceCounters;

					MatchedDistances( int totalNumInstances, int numQueryDistances )
						: matchedQueryDistancesByGlobalInstance( totalNumInstances, boost::dynamic_bitset<>( numQueryDistances ) )
						, matchedQueryDistanceCounters( numQueryDistances )
					{}

					void match( int globalInstanceIndex, int queryDistanceIndex ) {
						matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ][ queryDistanceIndex ] = true;
						++matchedQueryDistanceCounters[ queryDistanceIndex ];
					}

					int getTotalNumInstances() {
						return matchedQueryDistancesByGlobalInstance.size();
					}

					int getNumQueryDistances() {
						return matchedQueryDistanceCounters.size();
					}
				};

				struct Scores {
					std::vector<float> candidateInstanceScores;
					float maxPossibleWeightedPositiveScore;
					float maxPossibleWeightedNegativeScore;

					Scores( int totalNumInstances )
						: candidateInstanceScores( totalNumInstances )
						, maxPossibleWeightedPositiveScore()
						, maxPossibleWeightedNegativeScore()
					{}

					bool isEmpty() {
						return
								maxPossibleWeightedPositiveScore == 0.0f
							&&
								maxPossibleWeightedNegativeScore == 0.0f
						;
					}

					void integrateNeighborModelCandidateScores( float neighborModelWeight, const Scores &neighborModelCandidateScores ) {
						const int totalNumInstances = candidateInstanceScores.size();
						// update the total score
						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; globalInstanceIndex++ ) {
							candidateInstanceScores[ globalInstanceIndex ] +=
									neighborModelWeight
								*
									neighborModelCandidateScores.candidateInstanceScores[ globalInstanceIndex ]
							;
						}
						maxPossibleWeightedPositiveScore += neighborModelWeight * neighborModelCandidateScores.maxPossibleWeightedPositiveScore;
						maxPossibleWeightedNegativeScore += neighborModelWeight * neighborModelCandidateScores.maxPossibleWeightedNegativeScore;
					}

					void integrateMatchedDistances( MatchedDistances &matchedDistances ) {
						const int numQueryDistances = matchedDistances.getNumQueryDistances();
						const int totalNumInstances = matchedDistances.getTotalNumInstances();

						std::vector<float> scores( numQueryDistances );

						for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryDistances ; ++queryDistanceIndex ) {
							const float frequency = float( matchedDistances.matchedQueryDistanceCounters[ queryDistanceIndex ] + 1 ) / (totalNumInstances + 2);

							const float score = getMessageLength( frequency ); // + getMessageLength( 1.0f - frequency );

							scores[ queryDistanceIndex ] = score ;

							maxPossibleWeightedPositiveScore += score;
						}

						for( int globalInstanceIndex = 0 ; globalInstanceIndex < totalNumInstances ; ++globalInstanceIndex ) {
							for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryDistances ; ++queryDistanceIndex ) {
								if( matchedDistances.matchedQueryDistancesByGlobalInstance[ globalInstanceIndex ][ queryDistanceIndex ] ) {
									// this is a positive match because we have a matching query distance
									candidateInstanceScores[ globalInstanceIndex ] += scores[ queryDistanceIndex ];
								}
							}
						}
					}

					void integrateCorrelatedUnmatchedDistances( std::vector< int > &&correlatedGlobalInstanceIndices ) {
						const int totalNumInstances = candidateInstanceScores.size();

						const int numInstances = correlatedGlobalInstanceIndices.size();
						// frequency of the negative matches
						const float frequency = float( numInstances ) / (totalNumInstances + 2);

						const float negativeScore = getMessageLength( frequency );

						maxPossibleWeightedNegativeScore += negativeScore;

						for(
							auto correlatedGlobalInstanceIndex = correlatedGlobalInstanceIndices.begin() ;
							correlatedGlobalInstanceIndex != correlatedGlobalInstanceIndices.end() ;
							++correlatedGlobalInstanceIndex
						) {
							candidateInstanceScores[ *correlatedGlobalInstanceIndex ] -= negativeScore;
						}
					}

					float getInstanceScore( int globalInstanceIndex ) const {
						// alternative: use two ranges: 0.1..1.0 for positive scores and 0.0 to 0.1 for negative scores
						const float candidateInstanceScore = candidateInstanceScores[ globalInstanceIndex ];
						return combinePositiveAndNegativeScores( candidateInstanceScore, maxPossibleWeightedNegativeScore, maxPossibleWeightedPositiveScore );
					}
				};
			};
#endif
			// TODO: remove numMatchedDistances
			template< class Policy >
			UnmatchedDistances matchDistances(
				Id sceneNeighborModelId,
				typename Policy::Scores &neighborModelCandidateScores
			) {
				const int numSampledModels = database.sampledModelsById.size();
				const int totalNumInstances = database.getTotalNumInstances();

				// get the query volume's distances for the current neighbor model
				const Distances &queryDistances = queryDataset.getDistances( sceneNeighborModelId );
				const int numQueryDistances = queryDistances.size();

				const float neighborModelTolerance = Policy::getNeighborModelTolerance( this, sceneNeighborModelId );

				UnmatchedDistances unmatchedDistances;
				typename Policy::MatchedDistances matchedDistances( totalNumInstances, numQueryDistances );

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
								matchedDistances.match( globalInstanceIndex, queryDistanceIndex );

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

				neighborModelCandidateScores.integrateMatchedDistances( matchedDistances );

				return unmatchedDistances;
			}

			template< class Policy >
			void processUnmatchedDistances(
				Id sceneNeighborModelId,
				UnmatchedDistances &&unmatchedDistances,
				typename Policy::Scores &neighborModelCandidateScores
			) {
				// initialize short-hand references
				const int totalNumInstances = database.getTotalNumInstances();
				const float neighborModelTolerance = Policy::getNeighborModelTolerance( this, sceneNeighborModelId );

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
						std::vector<int> correlatedGlobalInstanceIndices;
						correlatedGlobalInstanceIndices.reserve( binEnd - binBegin );
						int numInstancesInBin = 0;
						for( auto binElement = binBegin ; binElement != binEnd ; ) {
							const int globalInstanceIndex = binElement->globalInstanceIndex;
							correlatedGlobalInstanceIndices.push_back( globalInstanceIndex );
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

						neighborModelCandidateScores.integrateCorrelatedUnmatchedDistances( std::move( correlatedGlobalInstanceIndices ) );
					}

					// use the left over mismatches for the next round until we're done with everything
					unmatchedDistances.swap( deferredUnmatchedDistances );
					deferredUnmatchedDistances.clear();
				}
			}

			template< class Policy >
			typename Policy::Scores matchAgainstNeighborModel( Id sceneNeighborModelId ) {
				const int totalNumInstances = database.getTotalNumInstances();

				// scores for the current neighborModelIndex
				typename Policy::Scores neighborModelCandidateScores( totalNumInstances );

				// first phase: matches + query mismatches
				std::vector< UnmatchedDistance > unmatchedDistances = matchDistances<Policy>(
					sceneNeighborModelId,
					neighborModelCandidateScores
				);

				// this is the number of merged mismatched distances
				processUnmatchedDistances<Policy>(
					sceneNeighborModelId,
					std::move( unmatchedDistances ),
					neighborModelCandidateScores
				);

				return neighborModelCandidateScores;
			}

			template< class Policy >
			Results executeWithPolicy() {
				const int totalNumInstances = database.getTotalNumInstances();

				// total score of all candidates
				typename Policy::Scores totalScores( totalNumInstances );

				// we iterate over all model ids and compare the query distances against all sampled models---one neighbor model at a time
				for( int sceneNeighborModelId = 0 ; sceneNeighborModelId < database.numIds ; ++sceneNeighborModelId ) {
					// = P( D | N )
					const float neighborModelWeight = Policy::getNeighborModelWeight( this, sceneNeighborModelId );

					if( neighborModelWeight == 0.0f ) {
						continue;
					}

					const typename Policy::Scores neighborModelCandidateScores = matchAgainstNeighborModel<Policy>( sceneNeighborModelId );

					totalScores.integrateNeighborModelCandidateScores( neighborModelWeight, neighborModelCandidateScores );
				}

				// compute the final score and store it in our results data structure
				if( !totalScores.isEmpty() )	{
					// determine the best model using argmax
					const int numSampledModels = database.getNumSampledModels();

					Results results;

					int globalInstanceIndex = 0;
					for( int candidateModelIndex = 0 ; candidateModelIndex < numSampledModels ; ++candidateModelIndex ) {
						const SampledModel &candidateModel = database.sampledModelsById[ candidateModelIndex ].second;
						const int numCandidateInstances = candidateModel.instances.size();

						// apply an argmax on all instances
						float bestCandidateInstanceScore = 0;

						const auto candidateInstances_end = globalInstanceIndex + numCandidateInstances;
						for( ; globalInstanceIndex < candidateInstances_end ; ++globalInstanceIndex ) {
							bestCandidateInstanceScore = std::max( bestCandidateInstanceScore, totalScores.getInstanceScore( globalInstanceIndex ) );
						}

						const auto sceneCandidateModelId = database.sampledModelsById[ candidateModelIndex ].first;
						//if( database.modelDatabase->informationById[ sceneCandidateModelId ].diagonalLength > 0 ) {
						results.push_back( Result( bestCandidateInstanceScore, sceneCandidateModelId ) );
					}
										
					return results;
				}
				else {
					return Results();
				}
			}

			Results execute() {
				return executeWithPolicy< JaccardIndexPolicy >();
			}
		};
	};
}

#pragma warning( pop )