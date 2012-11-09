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
template< typename Results >
struct ValidationDataResults {
	Validation::NeighborhoodSettings settings;

	float maxFrequency;
	Results uniformWeight, importanceWeight, jaccardIndex;

	SERIALIZER_DEFAULT_IMPL(
		(settings)
		(maxFrequency)
		(uniformWeight)
		(importanceWeight)
		(jaccardIndex)
	)
};

// per validation data
struct ExpectationsResult {
	Validation::NeighborhoodSettings settings;

	float maxFrequency;
	float uniformWeight, importanceWeight, jaccardIndex;

	SERIALIZER_DEFAULT_IMPL( 
		(settings)
		(maxFrequency)
		(uniformWeight)
		(importanceWeight)
		(jaccardIndex)
	)
};

typedef std::vector< ExpectationsResult > ExpectationsResults;

template<typename ExecutionKernel>
void executeKernel( const Neighborhood::NeighborhoodDatabaseV2 &neighborhoodDatabase, const Validation::NeighborhoodData &validationData, float &expectationResult, RankResults &rankResults ) {
	const float tolerance = validationData.settings.positionVariance + 0.25;

	const int numSamples = validationData.queryDatasets.size();
	rankResults.resize( numSamples );

	int rankSum = 0;

#pragma omp parallel for reduction(+ : rankSum)
	for( int sampleIndex = 0 ; sampleIndex < validationData.queryDatasets.size() ; ++sampleIndex ) {
		std::cout << "*";
		const int modelIndex = validationData.queryInfos[ sampleIndex ];

		const auto &queryData = validationData.queryDatasets[ sampleIndex ];

		const auto results = ExecutionKernel::execute( neighborhoodDatabase, tolerance, queryData );

		rankResults[ sampleIndex ] = results.size();
		for( int resultIndex = 0 ; resultIndex < results.size() ; ++resultIndex ) {
			if( results[ resultIndex ].second == modelIndex ) {
				rankResults[ sampleIndex ] = resultIndex;
				rankSum += resultIndex;
				break;
			}
		}
	}

	const float rankExpectation = double( rankSum ) /  validationData.queryDatasets.size();
	expectationResult = rankExpectation;
}

void testValidationData( 
	const Neighborhood::NeighborhoodDatabaseV2 &neighborhoodDatabase,
	const std::string &validationDataPath,
	ExpectationsResults &expectationsResults
) {
	Validation::NeighborhoodData validationData = Validation::NeighborhoodData::load( buildPath( validationDataPath + validationDataName ) );

	ExpectationsResult expectationResult;
	ValidationDataResults<RankResults> validationDataRanks;
	
	expectationResult.settings = validationData.settings;
	validationDataRanks.settings = validationData.settings;

	const float maxFrequency = validationData.instanceCounts.calculateRankExpectation();
	expectationResult.maxFrequency = maxFrequency;
	validationDataRanks.maxFrequency = maxFrequency;

	log( boost::format(
		"max distance %f\nposition variance %f\nnum samples %i" )
		% validationData.settings.maxDistance
		% validationData.settings.positionVariance
		% validationData.settings.numSamples
	 );

	log( boost::format( "num queries: %i" ) % validationData.queryDatasets.size() );
	log( boost::format( "maxFrequency // rank expectation: %f" ) % maxFrequency );

	executeKernel<UniformWeight_ExecutionKernel>( neighborhoodDatabase, validationData, expectationResult.uniformWeight, validationDataRanks.uniformWeight );
	log( boost::format( "uniformWeight // rank expectation: %f" ) % expectationResult.uniformWeight );

	executeKernel<ImportanceWeight_ExecutionKernel>( neighborhoodDatabase, validationData, expectationResult.importanceWeight, validationDataRanks.importanceWeight );
	log( boost::format( "importanceWeight // rank expectation: %f" ) % expectationResult.importanceWeight );

	executeKernel<JaccardIndex_ExecutionKernel>( neighborhoodDatabase, validationData, expectationResult.jaccardIndex, validationDataRanks.jaccardIndex );
	log( boost::format( "jaccardIndex // rank expectation: %f" ) % expectationResult.jaccardIndex );

	{
		const std::string resultFilePath = buildPath(
			boost::str( boost::format( "%s%f_%f_%i.rankResults.wml" ) 
				% resultsPath
				% validationData.settings.maxDistance
				% validationData.settings.positionVariance
				% validationData.settings.numSamples
			)
		);

		Serializer::TextWriter writer( resultsPath );
		Serializer::put( writer, validationDataRanks );
	}

	expectationsResults.push_back( expectationResult );

	{
		const std::string resultFilePath = buildPath(
			boost::str( boost::format( "%sexpectationsResults.wml" ) 
				% resultsPath
			)
		);

		Serializer::TextWriter writer( resultsPath );
		Serializer::put( writer, expectationsResults );
	}
}

void testNeighborDatabase(
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
		testValidationData( neighborhoodDatabase, validationDataPaths[i], expectationsResults );
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
		"5/2/"

		"10/0/",
		"10/2/",
		"10/4/"

		"20/0/",
		"20/2/",
		"20/4/",
		"20/8/"

		"40/0/",
		"40/2/",
		"40/4/",
		"40/8/"
	};

	ExpectationsResults expectationsResults;

	int index = 0;
	testNeighborDatabase( modelDatabase, "5/", 3, &paths[index], expectationsResults );
	index += 3;

	testNeighborDatabase( modelDatabase, "10/", 3, &paths[index], expectationsResults );
	index += 3;

	testNeighborDatabase( modelDatabase, "20/", 4, &paths[index], expectationsResults );
	index += 4;

	testNeighborDatabase( modelDatabase, "40/", 4, &paths[index], expectationsResults );
}