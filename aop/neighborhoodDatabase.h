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
			queryDataset( tolerance, maxDistance, sortedDataset )
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
							score += 1.0;
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
							score += 1.0;
							continue;
						}
						else if( variance( binAccumulator ) == 0.0 ) {
							if( mean( binAccumulator ) < 0.05 ) {
								score += 1;
							}
							continue;
						}
						else {
							normal_distribution<float> normal(
								mean( binAccumulator ),
								sqrt( variance( binAccumulator ) )
							);

							if( cdf( normal, 0.5f ) - cdf( normal, -0.5f ) ) {
								score += 1;
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