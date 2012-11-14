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
#include "boost/range/algorithm/sort.hpp"

using namespace Neighborhood;

/* base
 *	modelDB
 *		neighborDB
 *			validationData
 *
 * results are stored next to modelDB
 */

const std::string defaultConfigName = "neighborhoodValidation.wml";

struct Config {
	// base names for dirs
	std::string base;
	// base + / + results
	std::string results;

	// filenames
	std::string modelDatabase;
	std::string neighborhoodDatabase;
	std::string validationData;

	// basic config
	int sampleSelector;

	// runs
	typedef std::vector< std::string > Jobs;
	typedef std::pair< std::string, Jobs > VarianceJobs;
	typedef std::vector< VarianceJobs > DistanceVarianceJobs;
	DistanceVarianceJobs jobs;

	Config()
		: sampleSelector()
		, base( "C:/Andreas/neighborhoodValidationData" )
		, results( "results" )
		, modelDatabase( "modelDatabase" )
		, neighborhoodDatabase( "neighborhoodDatabaseV2" )
		, validationData( "neighborhood.validationData" )
	{
	}

	SERIALIZER_DEFAULT_IMPL(
		(sampleSelector)
		(base)
		(results)
		(modelDatabase)
		(neighborhoodDatabase)
		(validationData)
		(jobs)
	)

	std::string buildPath( const std::string &filePath ) const {
		const std::string path = boost::str(
			boost::format( "%s/%s")
			% base
			% filePath
		);
		log( boost::format( "Accessing %s" ) % path );
		return path;
	}

	std::string buildModelDatabasePath() const {
		return buildPath( modelDatabase );
	}

	std::string buildResultPath( const std::string &file ) const {
		return buildPath( results + "/" + file );
	}

	std::string buildNeighborhoodDatabasePath( const std::string &distancePath ) const {
		return buildPath( distancePath + "/" + neighborhoodDatabase );
	}

	std::string buildValidationDataPath( const std::string &distancePath, const std::string &variancePath ) const {
		return buildPath( distancePath + "/" + variancePath + "/" + validationData );
	}
};

Config config;

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
	RankResults &rankResults,
	FullResults &unsortedResults
) {
	const float tolerance = 5.0; // validationData.settings.positionVariance + 0.25;

	const int numSamples = validationData.settings.numSamples;
	const int numTotalSamples = validationData.queryDatasets.size() / numSamples;
	rankResults.resize( numTotalSamples );
	unsortedResults.resize( numTotalSamples );

	int rankSum = 0;

	boost::timer::cpu_timer cpuTimer;

#pragma omp parallel for reduction(+ : rankSum)
	for( int sampleIndex = 0 ; sampleIndex < numTotalSamples ; sampleIndex++  ) {
		const int queryIndex = sampleIndex * numSamples + sampleSelector;
		const int modelIndex = validationData.queryInfos[ queryIndex ];

		const auto &queryData = validationData.queryDatasets[ queryIndex ];

		Neighborhood::Results results = ExecutionKernel::execute( neighborhoodDatabase, tolerance, queryData );
		//Neighborhood::Results results;
		unsortedResults[ sampleIndex ].first = modelIndex;
		unsortedResults[ sampleIndex ].second = results;

		boost::sort( results, std::greater< Neighborhood::Result >() );

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

	const float rankExpectation = double( rankSum ) / numTotalSamples;
	expectationResult = rankExpectation;

	log( boost::format( "rankExpectation: %f" ) % rankExpectation, 1 );
	log( boost::format( "timer: %s" ) % format( timerResults ), 1 );
}

void testValidationData(
	int sampleSelector,
	const Neighborhood::NeighborhoodDatabaseV2 &neighborhoodDatabase,
	const std::string &validationDataFilePath,
	ExpectationsResults &expectationsResults
) {
	Validation::NeighborhoodData validationData;

	{
		bool success = Validation::NeighborhoodData::load( validationDataFilePath, validationData );

		if( !success ) {
			logError( boost::format( "Failed to load validationData '%s'!\n" ) % validationDataFilePath );
			return;
		}
	}

	ExpectationsResult expectationResult;
	ValidationDataResults<RankResults> validationDataRanks;
	ValidationDataResults<FullResults> fullResults;

	expectationResult.settings = validationData.settings;
	fullResults.settings = validationDataRanks.settings = validationData.settings;

	expectationResult.sampleSelector = sampleSelector;
	fullResults.sampleSelector = validationDataRanks.sampleSelector = sampleSelector;

	const float maxFrequency_expectedRank = validationData.instanceCounts.calculateRankExpectation();
	expectationResult.maxFrequency_expectedRank = maxFrequency_expectedRank;
	fullResults.maxFrequency_expectedRank = validationDataRanks.maxFrequency_expectedRank = maxFrequency_expectedRank;

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
			validationDataRanks.results.uniformWeight,
			fullResults.results.uniformWeight
		);

		log( "importanceWeight" );
		executeKernel<ImportanceWeight_ExecutionKernel>(
			sampleSelector,
			neighborhoodDatabase,
			validationData,
			expectationResult.expectedRanks.importanceWeight,
			expectationResult.timers.importanceWeight,
			validationDataRanks.results.importanceWeight,
			fullResults.results.importanceWeight
		);

		log( "jaccardIndex" );
		executeKernel<JaccardIndex_ExecutionKernel>(
			sampleSelector,
			neighborhoodDatabase,
			validationData,
			expectationResult.expectedRanks.jaccardIndex,
			expectationResult.timers.jaccardIndex,
			validationDataRanks.results.jaccardIndex,
			fullResults.results.jaccardIndex
		);

		{
			const std::string resultFilePath = config.buildResultPath(
				boost::str( boost::format( "%i__%i_%i_%i.rankResults.wml" )
					% sampleSelector
					% int( validationData.settings.maxDistance )
					% int( validationData.settings.positionVariance )
					% validationData.settings.numSamples
				)
			);

			Serializer::TextWriter writer( resultFilePath );
			Serializer::write( writer, validationDataRanks );
		}
		{
			const std::string resultFilePath = config.buildResultPath(
				boost::str( boost::format( "%i__%i_%i_%i.fullResults.wml" )
					% sampleSelector
					% int( validationData.settings.maxDistance )
					% int( validationData.settings.positionVariance )
					% validationData.settings.numSamples
				)
			);

			Serializer::TextWriter writer( resultFilePath );
			Serializer::write( writer, fullResults );
		}
	}

	expectationsResults.push_back( expectationResult );

	{
		const std::string resultFilePath = config.buildResultPath(
			boost::str( boost::format( "%i_expectationsResults.wml" )
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
	const Config::VarianceJobs &jobs,
	ExpectationsResults &expectationsResults
) {
	Neighborhood::NeighborhoodDatabaseV2 neighborhoodDatabase;
	neighborhoodDatabase.modelDatabase = &modelDatabase;

	{
		const std::string path = config.buildNeighborhoodDatabasePath( jobs.first );
		bool success = neighborhoodDatabase.load( path );

		if( !success ) {
			logError( boost::format( "Failed to load neighborhoodDatabase '%s'!\n" ) % path );
			return;
		}
	}

	for( auto job = jobs.second.begin() ; job != jobs.second.end() ; ++job ) {
		const std::string path = config.buildValidationDataPath( jobs.first, *job );
		testValidationData( sampleSelector, neighborhoodDatabase, path, expectationsResults );
	}
}

void main( int argc, const char **argv ) {
	omp_set_num_threads( 12*4 );

	// load config
	std::string configName = defaultConfigName;
	if( argc == 2 ) {
		configName = argv[1];
	}

	{
		Serializer::TextWriter writer( "neighborhoodValidation_defaultConfig.wml" );
		Serializer::write( writer, config );
	}

	{
		Serializer::TextReader reader( configName );
		log( "loaded config" );
		log( wml::emit( reader.root ) );
		Serializer::read( reader, config );
	}

	ModelDatabase modelDatabase( nullptr );
	{
		bool success = modelDatabase.load( config.buildModelDatabasePath() );

		if( !success ) {
			logError( "Failed to load the modelDatabase!\n" );
			return;
		}
	}

	ExpectationsResults expectationsResults;

	for( auto distanceJobs = config.jobs.begin() ; distanceJobs != config.jobs.end() ; ++distanceJobs ) {
		testNeighborDatabase( config.sampleSelector, modelDatabase, *distanceJobs, expectationsResults );	
	}
}