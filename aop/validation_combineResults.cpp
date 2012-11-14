#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "validationStorage.h"
#include "logger.h"

#include <string>
#include "boost/format.hpp"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/algorithm/string.hpp>
#include "boost/range/algorithm/sort.hpp"



namespace NeighborhoodValidation {
	/* base
	 *	modelDB
	 *		neighborDB
	 *			validationData
	 *
	 * results are stored next to modelDB
	 */
	using namespace Neighborhood;

	const std::string defaultConfigName = "neighborhoodValidation.wml";

	typedef std::vector< std::pair< int, Results > > FullResults;
	typedef std::vector< int > RankResults;

	// per validation data
	template< typename Result >
	struct KernelResults {
		Result uniformWeight, importanceWeight, jaccardIndex;

		SERIALIZER_DEFAULT_IMPL(
			(uniformWeight)
			(importanceWeight)
			(jaccardIndex)
		)

		KernelResults()
			: uniformWeight()
			, importanceWeight()
			, jaccardIndex()
		{}
	};

	template< typename Results >
	struct ValidationDataResults {
		Validation::NeighborhoodSettings settings;
		int sampleSelector;

		float maxFrequency_expectedRank;
		KernelResults<Results> results;

		SERIALIZER_DEFAULT_IMPL(
			(settings)
			(maxFrequency_expectedRank)
			(results)
		)
	};

	typedef boost::timer::cpu_times TimerResults;

	// per validation data
	struct ExpectationsResult {
		Validation::NeighborhoodSettings settings;
		int sampleSelector;
		int numQueries;
		float maxFrequency_expectedRank;
		KernelResults<TimerResults> timers;
		KernelResults<float> expectedRanks;

		SERIALIZER_DEFAULT_IMPL(
			(sampleSelector)
			(numQueries)
			(settings)
			(maxFrequency_expectedRank)
			(expectedRanks)
			(timers)
		)

		ExpectationsResult()
			: sampleSelector(-1)
			, numQueries()
			, maxFrequency_expectedRank()
			, timers()
			, expectedRanks()
		{
		}
	};

	typedef std::vector< ExpectationsResult > ExpectationsResults;
}

namespace ProbeValidation {
	using namespace ProbeContext;

	const std::string defaultConfigName = "probeValidation.wml";

	//////////////////////////////////////////////////////////////////////////
	typedef std::vector< std::pair< int, Neighborhood::Results > > FullResults;
	typedef std::vector< int > RankResults;

	// per validation data
	template< typename Result >
	struct KernelResults {
		Result uniformBidirectional, importanceBidirectional, uniformConfiguration, importanceConfiguration,
			fastUniformBidirectional, fastImportanceBidirectional, fastUniformConfiguration;

		SERIALIZER_DEFAULT_IMPL(
			(uniformBidirectional)
			(importanceBidirectional)
			(uniformConfiguration)
			(importanceConfiguration)
			(fastUniformBidirectional)
			(fastUniformConfiguration)
			(fastImportanceBidirectional)
		)

		KernelResults()
			: uniformBidirectional()
			, importanceBidirectional()
			, uniformConfiguration()
			, importanceConfiguration()
			, fastUniformBidirectional()
			, fastImportanceBidirectional()
			, fastUniformConfiguration()
		{}
	};

	struct ValidatorInfo {
		int numCores, numMachines, machineIndex;
		int sampleSelector;

		ValidatorInfo()
			: numCores()
			, numMachines()
			, machineIndex()
			, sampleSelector()
		{
		}

		SERIALIZER_DEFAULT_IMPL(
			(sampleSelector)
			(numMachines)
			(machineIndex)
			(numCores)
		)

		static ValidatorInfo globalInfo;
	};

	ValidatorInfo ValidatorInfo::globalInfo;

	template< typename Results >
	struct ValidationDataResults {
		Validation::ProbeSettings settings;
		ValidatorInfo info;

		float maxFrequency_expectedRank;
		KernelResults<Results> results;

		SERIALIZER_DEFAULT_IMPL(
			(info)
			(settings)
			(maxFrequency_expectedRank)
			(results)
		)
	};

	typedef boost::timer::cpu_times TimerResults;

	// per validation data
	struct ExpectationsResult {
		Validation::ProbeSettings settings;
		ValidatorInfo info;

		int numQueries;
		float maxFrequency_expectedRank;
		KernelResults<TimerResults> timers;
		KernelResults<float> expectedRanks;

		SERIALIZER_DEFAULT_IMPL(
			(info)
			(numQueries)
			(settings)
			(maxFrequency_expectedRank)
			(expectedRanks)
			(timers)
		)

		ExpectationsResult()
			: info()
			, numQueries()
			, maxFrequency_expectedRank()
			, timers()
			, expectedRanks()
		{
		}
	};

	typedef std::vector< ExpectationsResult > ExpectationsResults;
}

SERIALIZER_DEFAULT_EXTERN_IMPL( boost::timer::cpu_times,
	(wall)
	(user)
	(system)
)
/*
struct Combination {
	std::string probePath;
	std::string neighborhoodPath;
};*/

struct CombinedRanks {
	std::string info;
	float averageRank;
	std::vector<int> ranks;

	SERIALIZER_DEFAULT_IMPL(
		(info)
		(averageRank)
		(ranks)
	)
};

const std::string basePath = "C:/Andreas/selectedProbeValidations/";
const std::string mergedDataBinary = "mergedResults.bin";
const std::string probeFullResultsFilename = "wakeup/probes/0__5_0_1.probe.fullResults.wml";
const std::string neighborhoodFullResultsFilename = "wakeup/neighborhood/0__10_0_1.fullResults.wml";
std::string resultsFilename = "combi .wml";

#if 1 // convert data to binary
void main( int argc, const char **argv ) {
	CombinedRanks combinedRanks;

	combinedRanks.info = probeFullResultsFilename + " " + neighborhoodFullResultsFilename;

	// load the probe results
	ProbeValidation::ValidationDataResults<ProbeValidation::FullResults> probesFullResults;
	{
		Serializer::TextReader reader( basePath + probeFullResultsFilename );
		Serializer::read( reader, probesFullResults );
	}
	NeighborhoodValidation::ValidationDataResults<NeighborhoodValidation::FullResults> neighborhoodFullResults;
	{
		Serializer::TextReader reader( basePath + neighborhoodFullResultsFilename );
		Serializer::read( reader, neighborhoodFullResults );
	}
	/*
	{
		Serializer::BinaryWriter writer( basePath + mergedDataBinary );
		Serializer::write( writer, probesFullResults );
		Serializer::write( writer, neighborhoodFullResults );
	}
}

#elif 1 // read in binary and merge results
void main( int argc, const char **argv ) {
	ProbeValidation::ValidationDataResults<ProbeValidation::FullResults> probesFullResults;
	NeighborhoodValidation::ValidationDataResults<NeighborhoodValidation::FullResults> neighborhoodFullResults;
	{
		Serializer::BinaryReader reader( basePath + mergedDataBinary );
		Serializer::read( reader, probesFullResults );
		Serializer::read( reader, neighborhoodFullResults );
	}
	*/
	const auto &probeResults = probesFullResults.results.fastUniformConfiguration;
	const auto &neighborhoodResults = neighborhoodFullResults.results.jaccardIndex;

	//resultsFilename += " imp";

	// should be the same
	log(
		boost::format( "%i entries probes, %i entries neighborhood " )
		% probeResults.size()
		% neighborhoodResults.size()
	);

	int numModels = 0;
	int maxModelIndex = 0;
	int currentModel = 0;
	for( auto result = probeResults.begin() ; result != probeResults.end() ; ++result ) {
		if( currentModel != result->first ) {
			currentModel = result->first;
			maxModelIndex = std::max( maxModelIndex, currentModel );
			++numModels;
		}
	}
	log( boost::format( "numModels: %i, maxModelIndex %i" ) % numModels % maxModelIndex );

	return; 
	// merge all results and create a new ranking
	float rankSum = 0.0f;

	const int instanceCount = probeResults.size();
	std::vector<int> ranks( instanceCount );
	for( int instanceIndex = 0 ; instanceIndex < instanceCount ; instanceIndex++ ) {
		std::vector< float > combinedScores( maxModelIndex + 1 );

		if( probeResults[ instanceIndex ].first != neighborhoodResults[ instanceIndex ].first ) {
			logError( "FUCK" );
			return;
		}
		const int expectedModelIndex = probeResults[ instanceIndex ].first;

		{
			const auto &probeQueryResult = probeResults[ instanceIndex ].second;
			for( auto modelResult = probeQueryResult.begin() ; modelResult != probeQueryResult.end() ; ++modelResult ) {
				combinedScores[ modelResult->second ] = modelResult->first;
			}
		}

		// combine with the neighborhood results
		{
			const auto &neighborhoodQueryResult = neighborhoodResults[ instanceIndex ].second;
			for( auto modelResult = neighborhoodQueryResult.begin() ; modelResult != neighborhoodQueryResult.end() ; ++modelResult ) {
				/*// ignore results that we dont have in
				if( modelResult->second > maxModelIndex ) {
					continue;
				}*/
				combinedScores[ modelResult->second ] *= modelResult->first;
			}
		}

		// create a Results list again
		Neighborhood::Results combinedResults( maxModelIndex + 1 );
		for( int modelIndex = 0 ; modelIndex <= maxModelIndex ; modelIndex++ ) {
			combinedResults[ modelIndex ].first = combinedScores[ modelIndex ];
			combinedResults[ modelIndex ].second = modelIndex;
		}

		// filter results
		boost::remove_erase_if( combinedResults, [] ( const Neighborhood::Result &result ) { return result.first == 0.0f; } );
		// sort
		boost::sort( combinedResults, std::greater< Neighborhood::Result >() );

		// determine the rank
		int rank = numModels;
		for( int resultIndex = 0 ; resultIndex < combinedScores.size() ; resultIndex++ ) {
			if( combinedResults[ resultIndex ].second == expectedModelIndex ) {
				rank = resultIndex;
				break;
			}
		}
		rankSum += rank;
		ranks[instanceIndex] = rank;
	}

	const float rankExpectation = rankSum / instanceCount;
	combinedRanks.averageRank = rankExpectation;
	combinedRanks.ranks = ranks;
	log( boost::format( "expectation: %f" ) % rankExpectation );

	// dump the results
	{
		Serializer::TextWriter writer( basePath + resultsFilename );

		Serializer::write( writer, combinedRanks );
	}
}
#endif