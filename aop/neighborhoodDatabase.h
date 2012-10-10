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

#include <boost/math/distributions/normal.hpp> // for normal_distribution
#include <boost/multi_array.hpp>

#include "boost/tuple/tuple.hpp"
#include "boost/tuple/tuple_comparison.hpp"

using boost::math::normal_distribution; // typedef provides default type is double.

struct NeighborhoodDatabase {
	typedef int Id;
	static const int INVALID_ID = -1;

	typedef std::pair< Id, float > IdDistancePair;
	typedef std::vector< IdDistancePair > RawDataset;

	struct SortedDataset {
		typedef std::pair< Id, std::vector< float > > IdDistancesPair;
		typedef std::vector< IdDistancesPair > DistancesById;

		SortedDataset( RawDataset &&rawDataset ) {
			boost::sort( rawDataset );

			for( auto idDistancePair = rawDataset.begin() ; idDistancePair != rawDataset.end() ; ) {
				int currentId = idDistancePair->first;

				distancesById.push_back( IdDistancesPair() );
				auto &idDistancesPair = distancesById.back();
				idDistancesPair.first = currentId;

				for( ; idDistancePair != rawDataset.end() && idDistancePair->first == currentId ; ++idDistancePair ) {
					idDistancesPair.second.push_back( idDistancePair->second );
				}
			}
		}

		const DistancesById &getDistancesById() const {
			return distancesById;
		}

	private:
		DistancesById distancesById;
	};

	struct Dataset {
		typedef std::pair< Id, std::vector< int > > IdBinsPair;

		static int getNumBins( float binWidth, float maxDistance ) {
			return 1 + 2 * int( ceil( maxDistance / binWidth ) );
		}
		
		Dataset( float binWidth, float maxDistance, const SortedDataset &sortedDataset ) :
			binWidth( binWidth ),
			maxDistance( maxDistance )
		{
			const float halfBinWidth = binWidth / 2;
			const int numBins = getNumBins( binWidth, maxDistance );

			const int numIds = sortedDataset.getDistancesById().size();
			binsById.resize( numIds );
			for( int i = 0 ; i < numIds ; i++ ) {
				const auto &idDistancePair = sortedDataset.getDistancesById()[ i ];

				auto &idBinPair = binsById[ i ];
				idBinPair.first = idDistancePair.first;
				
				idBinPair.second.resize( numBins );

				for( auto distance = idDistancePair.second.begin() ; distance != idDistancePair.second.end() ; ++distance ) {
					if( *distance >= maxDistance ) {
						break;
					}

					const int binIndex = int(*distance / binWidth) * 2 + 1;
					idBinPair.second[ binIndex ]++;

					const int otherBinIndex = int((*distance + halfBinWidth) / binWidth) * 2;
					idBinPair.second[ otherBinIndex ]++;
				}
			}
		}

		const std::vector< IdBinsPair > &getBinsById() const {
			return binsById;
		}

	private:
		float binWidth;
		float maxDistance;
		
		std::vector< IdBinsPair > binsById;
	};

	struct Entry {
		std::vector< SortedDataset > instances;

		void addInstance( SortedDataset &&sortedDataset ) {
			instances.emplace_back( std::move( sortedDataset ) );
		}
	};

	Entry &getEntryById( Id id ) {
		for( auto idEntryPair = entriesById.begin() ; idEntryPair != entriesById.end() ; ++idEntryPair ) {
			if( idEntryPair->first == id ) {
				return idEntryPair->second;
			}
		}

		entriesById.push_back( std::make_pair( id, Entry() ) );
		return entriesById.back().second;
	}

	struct Query {
		const NeighborhoodDatabase &neighborhoodDatabase;
		const Dataset queryDataset;
		
		const float tolerance;
		const float maxDistance;

		Query( const NeighborhoodDatabase &neighborhoodDatabase, float tolerance, float maxDistance, SortedDataset &&sortedDataset ) :
			neighborhoodDatabase( neighborhoodDatabase ),
			tolerance( tolerance ),
			maxDistance( maxDistance ),
			queryDataset( tolerance, maxDistance, std::move( sortedDataset ) )
		{
		}

		typedef std::pair< float, Id > ScoreIdPair;

		typedef std::vector< ScoreIdPair > Results;

		Results execute() {
			Results results;

			for( auto idEntryPair = neighborhoodDatabase.entriesById.begin() ; idEntryPair != neighborhoodDatabase.entriesById.end() ; ++idEntryPair ) {
				const float score = processEntry( idEntryPair->second );
				results.push_back( std::make_pair( score, idEntryPair->first ) );
			}

			boost::sort( results, std::greater< ScoreIdPair >() );
			return results;
		}

		float processEntry( const Entry &entry ) {
			using namespace boost::accumulators;

			typedef accumulator_set<
				float,
				features<
					tag::count,
					tag::mean,
					tag::variance
				>
			> Accumulator;

			std::map< Id, std::vector< Accumulator > > accumulatorsById;

			const int numBins = Dataset::getNumBins( tolerance, maxDistance );

			// init the accumulators
			for( auto instance = entry.instances.begin() ; instance != entry.instances.end() ; ++instance ) {
				const Dataset instanceDataset( tolerance, maxDistance, *instance );

				for( auto idBinsPair = instanceDataset.getBinsById().begin() ; idBinsPair != instanceDataset.getBinsById().end() ; ++idBinsPair ) {
					auto &accumulators = accumulatorsById[ idBinsPair->first ];
					accumulators.resize( numBins );

					for( int i = 0 ; i < idBinsPair->second.size() ; i++ ) {
						accumulators[i]( idBinsPair->second[ i ] );
					}
				}
			}

			// calculate the 'probability'
			auto idBinsPair = queryDataset.getBinsById().begin();

			float score = 0.0f;
			for( auto accumulator = accumulatorsById.begin() ; accumulator != accumulatorsById.end() ; ++accumulator ) {
				while(
					idBinsPair != queryDataset.getBinsById().end() && 
					idBinsPair->first < accumulator->first
				) {
					idBinsPair++;
				}
				if( idBinsPair != queryDataset.getBinsById().end() && idBinsPair->first == accumulator->first ) {
					// score
					for( int i = 0 ; i < numBins ; i++ ) {
						const auto &binAccumulator = accumulator->second[i];

						const int testCount = idBinsPair->second[i];

						if( count( binAccumulator ) == 0 ) {
							//score += 1.0;
							continue;
						}
						else if( variance( binAccumulator ) == 0.0 ) {
							if( fabs( testCount - mean( binAccumulator ) ) < 0.05 ) {
								score += 1;
							}
							continue;
						}
						else {
							normal_distribution<float> normal(
								mean( binAccumulator ),
								sqrt( variance( binAccumulator ) )
							);

						
							if( cdf( normal, testCount + 0.5f ) - cdf( normal, testCount - 0.5f ) > 0.5 ) {
								score += 1;
							}
						}
					}
				}
				else {
					// no query data found for this id, ie no such objects are around
					// score
					for( int i = 0 ; i < numBins ; i++ ) {
						const auto &binAccumulator = accumulator->second[i];

						if( count( binAccumulator ) == 0 ) {
							//score += 1.0;
							continue;
						}
						else if( variance( binAccumulator ) == 0.0 ) {
							if( mean( binAccumulator ) > 0.05 ) {
								score -= 1;
							}
							continue;
						}
						else {
							normal_distribution<float> normal(
								mean( binAccumulator ),
								sqrt( variance( binAccumulator ) )
							);

							if( cdf( normal, 0.5f ) - cdf( normal, -0.5f ) < 0.5) {
								score -= 1;
							}
						}
					}
				}
			}
			score /= numBins * accumulatorsById.size();
			return score;
		}
	};
	
#if 0
	struct Query {
		// problem with this implementation: the smaller tolerance the better, because 0 levels always match
		float maxDistance;

		float entryDistanceTolerance;
		float queryDistanceTolerance;

		const SortedDataset *queryData;
		const Entry *entry;

		enum EventType {
			ET_QUERY,
			ET_PUSH_QUERY = ET_QUERY,
			ET_POP_QUERY,
			ET_ENTRY, 
			ET_PUSH_ENTRY = ET_ENTRY,
			ET_POP_ENTRY
		};

		typedef std::pair< float, EventType > Event;

		float execute() {
			float score = 0.0;
			for( auto instance = entry->instances.begin() ; instance != entry->instances.end() ; ++instance ) {
				score += processInstance( *instance );
			}
			return score / entry->instances.size();
		}

		float processInstance( const SortedDataset &instance ) {
			auto instanceIdDistances = instance.getDistancesById().begin();
			auto queryIdDistances = queryData->getDistancesById().end();

			// TODO: adapt the code here to treat queries and instances asymmetrically [10/9/2012 kirschan2]
			float score = 0.0;
			int numIds = 0;
			while(
				instanceIdDistances != instance.getDistancesById().end() &&
				queryIdDistances != queryData->getDistancesById().end()
			) {
				while( instanceIdDistances->first < queryIdDistances->first ) {
					score += getMatchLength( std::vector<float>(), instanceIdDistances->second );
					instanceIdDistances++, numIds++;
				}
				while( instanceIdDistances->first > queryIdDistances->first ) {
					score += getMatchLength( queryIdDistances->second, std::vector<float>() );
					queryIdDistances++, numIds++;
				}
				if( instanceIdDistances->first == queryIdDistances->first ) {
					score += getMatchLength( queryIdDistances->second, instanceIdDistances->second );
					queryIdDistances++, instanceIdDistances++, numIds++;
				}
			}
			while( instanceIdDistances != instance.getDistancesById().end() ) {
				score += getMatchLength( std::vector<float>(), instanceIdDistances->second );
				instanceIdDistances++, numIds++;
			}
			while( queryIdDistances != queryData->getDistancesById().end() ) {
				score += getMatchLength( queryIdDistances->second, std::vector<float>() );
				queryIdDistances++, numIds++;
			}

			return score / numIds;
		}

		void pushEvents( float distanceTolerance, std::vector< Event > &events, const std::vector< float > &distances, EventType base ) {
			for( auto distance = distances.begin() ; distance != distances.end() ; ++distance ) {
				events.push_back( std::make_pair( *distance, base ) );	
				events.push_back( std::make_pair( *distance + distanceTolerance, base + 1 ) );
			}
		}

		float getMatchLength( const std::vector< float > &queryDistances, const std::vector<float> &entryDistance ) {
			float matchLength = 0.0f;

			std::vector< Event > events;
			events.reserve( (queryDistances.size() + entryDistance.size()) * 2 );
			pushEvents( queryDistanceTolerance, events, queryDistances, ET_QUERY );
			pushEvents( entryDistanceTolerance, events, entryDistance, ET_ENTRY );
			boost::sort( events );
			
			int entryLevel = 0;
			int queryLevel = 0;
			float lastDistance = 0;
			for( auto event = events.begin() ; event != events.end() ; ++event ) {
				const float currentDistance = event->first;

				if( currentDistance >= maxDistance ) {
					break;
				}

				if( entryLevel == queryLevel ) {
					matchLength += currentDistance - lastDistance;
				}

				switch( event->second ) {
				case ET_PUSH_ENTRY:
					entryLevel++;
					break;
				case ET_POP_ENTRY:
					entryLevel--;
					break;
				case ET_PUSH_QUERY:
					queryLevel++;
					break;
				case ET_POP_QUERY:
					queryLevel--;
					break;
				}

				lastDistance = currentDistance;
			}

			if( queryLevel == entryLevel ) {
				matchLength += maxDistance - lastDistance;
			}
		}
	};
#endif

	typedef std::pair< int, Entry > IdEntryPair;

	std::vector< IdEntryPair > entriesById;
};

struct NeighborhoodDatabaseV2 {
	typedef int Id;
	static const int INVALID_ID = -1;

	static int numIds;

	typedef std::vector< float > Distances;

	typedef std::pair< Id, float > IdDistancePair;
	typedef std::vector< IdDistancePair > RawDataset;

	typedef std::pair< float, Id > DistanceIdPair;

	typedef std::vector< Distances > DistancesById; // index with [id]!
	
	// make sure we add entries for all IDs for later
	struct SortedDataset {
		SortedDataset( RawDataset &&rawDataset ) {
			boost::sort( rawDataset );

			distancesById.resize( numIds );

			for( auto idDistancePair = rawDataset.begin() ; idDistancePair != rawDataset.end() ; ++idDistancePair ) {
				distancesById[ idDistancePair->first ].push_back( idDistancePair->second );
			}
		}

		const DistancesById &getDistancesById() const {
			return distancesById;
		}

	private:
		DistancesById distancesById;
	};
	
	struct Entry {
		std::vector< SortedDataset > instances;

		Entry() {}

		void addInstance( SortedDataset &&sortedDataset ) {
			instances.emplace_back( std::move( sortedDataset ) );
		}
	};

	typedef std::pair< int, Entry > IdEntryPair;

	std::vector< IdEntryPair > entriesById;

	Entry &getEntryById( Id id ) {
		for( auto idEntryPair = entriesById.begin() ; idEntryPair != entriesById.end() ; ++idEntryPair ) {
			if( idEntryPair->first == id ) {
				return idEntryPair->second;
			}
		}

		entriesById.push_back( std::make_pair( id, Entry() ) );
		return entriesById.back().second;
	}

	struct Query {
		const NeighborhoodDatabaseV2 &database;
		const SortedDataset queryDataset;
		
		const float queryTolerance;

		Query( const NeighborhoodDatabaseV2 &database, float queryTolerance, SortedDataset &&sortedDataset ) :
			database( database ),
			queryTolerance( queryTolerance ),
			queryDataset( std::move( sortedDataset ) )
		{
		}

		typedef std::pair< float, Id > ScoreIdPair;

		typedef std::vector< ScoreIdPair > Results;

		Results execute() {
			const int numEntries = database.entriesById.size();
			// total score of all entries
			std::vector<float> totalScore( numEntries );
			
			// we iterate over all ids
			for( int id = 0 ; id < numIds ; ++id ) {
				const float idTolerance = 1.0;

				// scores for the current id
				std::vector<float> idScore( numEntries );

				const auto &queryData = queryDataset.getDistancesById()[ id ];				
				const int numQueryBins = queryData.size();

				// (distance, entry, globalInstanceIndex)
				typedef boost::tuple< float, int, int > Mismatch;
				std::vector< Mismatch > mismatches;

				// first phase: matches + query mismatches 
				{
					// counts all matches for a certain query distance and entry
					boost::multi_array<int, 2> entryBins( boost::extents[numEntries][numQueryBins] );
					// counts all matches for a certain query distance
					std::vector<int> bins( numQueryBins );

					int globalInstanceIndex = 0;
					for( int entryIndex = 0 ; entryIndex < numEntries ; ++entryIndex ) {
						const Entry &entry = database.entriesById[ entryIndex ].second;

						for( int instanceIndex = 0 ; instanceIndex < entry.instances.size() ; ++instanceIndex, ++globalInstanceIndex ) {
							const Distances &instanceDistances = entry.instances[ instanceIndex ].getDistancesById()[ id ];
						
							auto instanceDistance = instanceDistances.begin();
							for( int queryDistanceIndex = 0 ; queryDistanceIndex < numQueryBins ; ++queryDistanceIndex ) {
								const float queryDistance = queryData[ queryDistanceIndex ];
								for( ; instanceDistance != instanceDistances.end() && *instanceDistance < queryDistance - idTolerance ; ++instanceDistance ) {
									// no query that can match this instance distance, so its a mismatch
									mismatches.emplace_back( Mismatch( *instanceDistance, entryIndex, globalInstanceIndex ) );
								}

								if( instanceDistance == instanceDistances.end() ) {
									break;
								}

								// can this instance distance be matched by this query?
								if( *instanceDistance <= queryDistance + idTolerance + queryTolerance ) {
									// match
									++bins[ queryDistanceIndex ];
									++entryBins[ entryIndex ][ queryDistanceIndex ];

									// we're done with this instance distance as well
									++instanceDistance;
								}
							}
							// the remaining instance elements are mismatches, too
							for( ; instanceDistance != instanceDistances.end() ; ++instanceDistance ) {
								mismatches.emplace_back( Mismatch( *instanceDistance, entryIndex, globalInstanceIndex ) );
							}
						}
					}

					const int numTotalInstances = globalInstanceIndex;
					if( numTotalInstances != 0 ) {
						// calculate the match probabilities
						for( int binIndex = 0 ; binIndex < numQueryBins ; ++binIndex ) {
							if( bins[ binIndex ] == 0 ) {
								continue;
							}

							for( int entryIndex = 0 ; entryIndex < numEntries ; ++entryIndex ) {
								const Entry &entry = database.entriesById[ entryIndex ].second;

								const float conditionalProbability = float( entryBins[ entryIndex ][ binIndex ] ) / bins[ binIndex ];

								idScore[ entryIndex ] += conditionalProbability;
							}
						}
					}
				}

				int numMismatchBins = 0;
				// second pass: process mismatches
				while( !mismatches.empty() ) {
					// TODO: write predicates and custom structs, so we can use one type for bin elements and mismatches
					// sort the mismatches by distance first
					boost::sort( mismatches );

					std::vector< Mismatch > leftOverMismatches;
					leftOverMismatches.reserve( mismatches.size() );

					// (globalInstanceIndex, entry, distance)
					typedef boost::tuple< int, int, float > BinElement; 
					std::vector< BinElement > binElements;
					binElements.reserve( mismatches.size() );

					// TODO: we can create and sort the bin in-place then.. [10/10/2012 kirschan2]

					// add all mismatches into bins
					for( auto mismatch = mismatches.cbegin() ; mismatch != mismatches.cend() ; ) {
						const float leftDistance = mismatch->get<0>();
						do {
							binElements.emplace_back( BinElement( mismatch->get<2>(), mismatch->get<1>(), mismatch->get<0>() ) );
							++mismatch;
						} while( 
							mismatch != mismatches.end() &&
							mismatch->get<0>() <= leftDistance + 2 * idTolerance + queryTolerance 
						);
						
						// sort by globalInstanceIndex and then entry (globalInstanceIndex is already sorted by entry implicitly)
						boost::sort( binElements );
						numMismatchBins++;

						// count the number of unique instance elements
						int uniqueInstanceElements = 0;
						for( auto binElement = binElements.cbegin() ; binElement != binElements.cend() ; ) {
							const int globalInstanceIndex = binElement->get<0>();
							++binElement;
							++uniqueInstanceElements;

							// skip over additional mismatches from globalInstanceIndex in this bin
							// and add them to the left over mismatches, because we cant process them this round
							while( 
								binElement != binElements.cend() &&
								binElement->get<0>() == globalInstanceIndex
							) {
								leftOverMismatches.emplace_back( Mismatch( binElement->get<2>(), binElement->get<1>(), binElement->get<0>() ) );
								binElement++;
							}
						}

						// count the entry elements in this bin and calculate the probability of the mismatch
						for( auto binElement = binElements.cbegin() ; binElement != binElements.cend() ; ) {
							const int entryIndex = binElement->get<1>();

							// count the unique elements with this entryIndex
							int uniqueEntryElements = 0;
							do {
								const int globalInstanceIndex = binElement->get<0>();
								do {
									binElement++;
								}
								while(
									binElement != binElements.cend() &&
									binElement->get<0>() == globalInstanceIndex
								);

								uniqueEntryElements++;
							}
							while(
								binElement != binElements.cend() &&
								binElement->get<1>() == entryIndex
							);

							// calculate the conditional probability that we have a match
							const float conditionalProbability = float( uniqueEntryElements ) / uniqueInstanceElements;
							// mismatch, so we add the complement
							idScore[ entryIndex ] += 1.0 - conditionalProbability;
						}

						// reset binElements for the next round
						binElements.clear();
					}

					// use the left over mismatches for the next round until we're done with everything
					mismatches.swap( leftOverMismatches );
				}

				// update the total score
				for( int entryIndex = 0 ; entryIndex < numEntries ; ++entryIndex ) {
					totalScore[ entryIndex ] += idScore[ entryIndex ] / (numQueryBins +  numMismatchBins);
				}
			}

			// compute the final score and store it in our results data structure
			{
				Results results;
				for( int entryIndex = 0 ; entryIndex < numEntries ; ++entryIndex ) {
					const auto entryId = database.entriesById[ entryIndex ].first;

					const float score = totalScore[ entryIndex ] / numIds;
					results.push_back( ScoreIdPair( score, entryId ) );
				}
				boost::sort( results, std::greater< ScoreIdPair >() );
				return results;
			}
		}
	};
};