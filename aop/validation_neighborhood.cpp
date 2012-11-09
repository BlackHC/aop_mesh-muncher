#include "validationStorage.h"

#include "modelDatabase.h"
#include "neighborhoodDatabase.h"
#include "validation.h"

#include <string>
#include "boost/format.hpp"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/algorithm/string.hpp>

using namespace Neighborhood;

/* base
 *	modelDB
 *		neighborDB
 *			validationData
 *
 * results are stored next to modelDB
 */

const std::string basePath = "C:/Andreas/neighborhoodValidationData";
const std::string modelDatabaseName = "modelDatabase";
const std::string databaseName = "neighborhoodDatabaseV2";
const std::string validationDataName = "neighborhood.validationData";

const std::string resultsPath = "results/";

std::string buildPath( const std::string &filePath ) {
	const std::string path = boost::str(
		boost::format( "%s/%s")
		% basePath
		% filePath
	);
	log( boost::format( "Accessing %s" ) % path );
	return path;
}

struct UniformWeight_ExecutionKernel {
	static Neighborhood::Results execute( const Neighborhood::NeighborhoodDatabaseV2 &neighborhoodDatabase, float tolerance, RawIdDistances queryData ) {
		Neighborhood::NeighborhoodDatabaseV2::Query query( neighborhoodDatabase, tolerance, std::move( queryData ) );
		const Neighborhood::Results results = query.executeWithPolicy<Neighborhood::NeighborhoodDatabaseV2::Query::UniformWeightPolicy>();
		return results;
	}

	static std::string getInfoString() {
		return "UniformWeightPolicy";
	}
};

struct ImportanceWeight_ExecutionKernel {
	static Neighborhood::Results execute( const Neighborhood::NeighborhoodDatabaseV2 &neighborhoodDatabase, float tolerance, RawIdDistances queryData ) {
		Neighborhood::NeighborhoodDatabaseV2::Query query( neighborhoodDatabase, tolerance, std::move( queryData ) );
		const Neighborhood::Results results = query.executeWithPolicy<Neighborhood::NeighborhoodDatabaseV2::Query::ImportanceWeightPolicy>();
		return results;
	}

	static std::string getInfoString() {
		return "ImportanceWeightPolicy";
	}
};

struct JaccardIndex_ExecutionKernel {
	static Neighborhood::Results execute( const Neighborhood::NeighborhoodDatabaseV2 &neighborhoodDatabase, float tolerance, RawIdDistances queryData ) {
		Neighborhood::NeighborhoodDatabaseV2::Query query( neighborhoodDatabase, tolerance, std::move( queryData ) );
		const Neighborhood::Results results = query.executeWithPolicy<Neighborhood::NeighborhoodDatabaseV2::Query::JaccardIndexPolicy>();
		return results;
	}

	static std::string getInfoString() {
		return "JaccardIndexPolicy";
	}
};


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

SERIALIZER_DEFAULT_EXTERN_IMPL( boost::timer::cpu_times,
	(wall)
	(user)
	(system)
)

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

template<typename ExecutionKernel>
void executeKernel(
	int sampleSelector,
	const Neighborhood::NeighborhoodDatabaseV2 &neighborhoodDatabase,
	const Validation::NeighborhoodData &validationData,
	float &expectationResult,
	TimerResults &timerResults,
	RankResults &rankResults
) {
	const float tolerance = validationData.settings.positionVariance + 0.25;

	const int numSamples = validationData.settings.numSamples;
	const int numTotalSamples = validationData.queryDatasets.size() / numSamples;
	rankResults.resize( numTotalSamples );

	int rankSum = 0;

	boost::timer::cpu_timer cpuTimer;

#pragma omp parallel for reduction(+ : rankSum)
	for( int sampleIndex = 0 ; sampleIndex < numTotalSamples ; sampleIndex++  ) {
		const int queryIndex = sampleIndex * numSamples + sampleSelector;
		const int modelIndex = validationData.queryInfos[ queryIndex ];

		const auto &queryData = validationData.queryDatasets[ queryIndex ];

		const auto results = ExecutionKernel::execute( neighborhoodDatabase, tolerance, queryData );
		//Neighborhood::Results results;

		rankResults[ sampleIndex ] = results.size();
		for( int resultIndex = 0 ; resultIndex < results.size() ; ++resultIndex ) {
			if( results[ resultIndex ].second == modelIndex ) {
				rankResults[ sampleIndex ] = resultIndex;
				rankSum += resultIndex;
				break;
			}
		}

		if( (sampleIndex % 100) == 0 ) {
			std::cout << "*";
		}
	}
	timerResults = cpuTimer.elapsed();
	std::cout << "\n";

	const float rankExpectation = double( rankSum ) /  validationData.queryDatasets.size();
	expectationResult = rankExpectation;

	log( boost::format( "rankExpectation: %f" ) % rankExpectation, 1 );
	log( boost::format( "timer: %s" ) % format( timerResults ), 1 );
}

void testValidationData(
	int sampleSelector,
	const Neighborhood::NeighborhoodDatabaseV2 &neighborhoodDatabase,
	const std::string &validationDataPath,
	ExpectationsResults &expectationsResults
) {
	Validation::NeighborhoodData validationData;

	{
		const std::string validationDataFilePath = buildPath( validationDataPath + validationDataName );
		bool success = Validation::NeighborhoodData::load( validationDataFilePath, validationData );

		if( !success ) {
			logError( boost::format( "Failed to load validationData '%s'!\n" ) % validationDataFilePath );
			return;
		}
	}

	ExpectationsResult expectationResult;
	ValidationDataResults<RankResults> validationDataRanks;

	expectationResult.settings = validationData.settings;
	validationDataRanks.settings = validationData.settings;

	expectationResult.sampleSelector = sampleSelector;
	validationDataRanks.sampleSelector = sampleSelector;

	const float maxFrequency_expectedRank = validationData.instanceCounts.calculateRankExpectation();
	expectationResult.maxFrequency_expectedRank = maxFrequency_expectedRank;
	validationDataRanks.maxFrequency_expectedRank = maxFrequency_expectedRank;
	
	if( sampleSelector < validationData.settings.numSamples ) {
		expectationResult.numQueries = validationData.queryDatasets.size() / validationData.settings.numSamples;

		log( boost::format(
			"max distance %f\nposition variance %f\nnum samples %i" )
			% validationData.settings.maxDistance
			% validationData.settings.positionVariance
			% validationData.settings.numSamples
		 );

		log( boost::format( "num queries: %i" ) % validationData.queryDatasets.size() );
		log( boost::format( "maxFrequency // rank expectation: %f" ) % maxFrequency_expectedRank );

		log( "uniformWeight" );
		executeKernel<UniformWeight_ExecutionKernel>(
			sampleSelector,
			neighborhoodDatabase,
			validationData,
			expectationResult.expectedRanks.uniformWeight,
			expectationResult.timers.uniformWeight,
			validationDataRanks.results.uniformWeight
		);

		log( "importanceWeight" );
		executeKernel<ImportanceWeight_ExecutionKernel>(
			sampleSelector,
			neighborhoodDatabase,
			validationData,
			expectationResult.expectedRanks.importanceWeight,
			expectationResult.timers.importanceWeight,
			validationDataRanks.results.importanceWeight
		);

		log( "jaccardIndex" );
		executeKernel<JaccardIndex_ExecutionKernel>(
			sampleSelector,
			neighborhoodDatabase,
			validationData,
			expectationResult.expectedRanks.jaccardIndex,
			expectationResult.timers.jaccardIndex,
			validationDataRanks.results.jaccardIndex
		);

		{
			const std::string resultFilePath = buildPath(
				boost::str( boost::format( "%s%i__%i_%i_%i.rankResults.wml" )
					% resultsPath
					% sampleSelector
					% int( validationData.settings.maxDistance )
					% int( validationData.settings.positionVariance )
					% validationData.settings.numSamples
				)
			);

			Serializer::TextWriter writer( resultFilePath );
			Serializer::write( writer, validationDataRanks );
		}
	}

	expectationsResults.push_back( expectationResult );

	{
		const std::string resultFilePath = buildPath(
			boost::str( boost::format( "%s%i_expectationsResults.wml" )
				% resultsPath
				% sampleSelector
			)
		);

		Serializer::TextWriter writer( resultFilePath );
		Serializer::write( writer, expectationsResults );
	}
}

void testNeighborDatabase(
	int sampleSelector,
	ModelDatabase &modelDatabase,
	const char *neighborDatabasePath,
	int numValidationDataPaths,
	const char **validationDataPaths,
	ExpectationsResults &expectationsResults
) {
	Neighborhood::NeighborhoodDatabaseV2 neighborhoodDatabase;
	neighborhoodDatabase.modelDatabase = &modelDatabase;

	{
		bool success = neighborhoodDatabase.load( buildPath( neighborDatabasePath + databaseName ) );

		if( !success ) {
			logError( boost::format( "Failed to load neighborhoodDatabase '%s'!\n" ) % neighborDatabasePath );
			return;
		}
	}

	for( int i = 0 ; i < numValidationDataPaths ; i++ ) {
		testValidationData( sampleSelector, neighborhoodDatabase, validationDataPaths[i], expectationsResults );
	}
}

void main() {
	omp_set_num_threads( 12*4 );

	ModelDatabase modelDatabase( nullptr );
	{
		bool success = modelDatabase.load( buildPath( modelDatabaseName ) );

		if( !success ) {
			logError( "Failed to load the modelDatabase!\n" );
			return;
		}
	}

	const char *paths[] = {
		"5/0/",
		"5/1/",
		"5/2/",

		"10/0/",
		"10/2/",
		"10/4/",

		"20/0/",
		"20/2/",
		"20/4/",
		"20/8/",

		"40/0/",
		"40/2/",
		"40/4/",
		"40/8/"
	};
	const int sampleSelector = 0;

	ExpectationsResults expectationsResults;

	int index = 0;
	testNeighborDatabase( sampleSelector, modelDatabase, "5/", 3, &paths[index], expectationsResults );
	index += 3;

	testNeighborDatabase( sampleSelector, modelDatabase, "10/", 3, &paths[index], expectationsResults );
	index += 3;

	testNeighborDatabase( sampleSelector, modelDatabase, "20/", 4, &paths[index], expectationsResults );
	index += 4;

	testNeighborDatabase( sampleSelector, modelDatabase, "40/", 4, &paths[index], expectationsResults );
}