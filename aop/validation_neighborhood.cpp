#include "modelDatabase.h"
#include "neighborhoodDatabase.h"
#include "validation.h"

#include <string>
#include <boost/format.hpp>

using namespace Neighborhood;

const std::string basePath = "neighborhoodValidationData";
const std::string modelDatabaseName = "modelDatabase";
const std::string databaseName = "neighborhoodDatabaseV2";
const std::string validationDataName = "neighborhood.validationData";

std::string buildPath( int distance, const std::string &filename ) {
	const std::string path = boost::str(
		boost::format( "%s/%i/%s")
		% basePath
		% distance
		% filename
	);
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

void main() {
	int distance = 5;

	bool success = true;

	ModelDatabase modelDatabase( nullptr );
	success &= modelDatabase.load( buildPath( distance, modelDatabaseName ) );

	Neighborhood::NeighborhoodDatabaseV2 neighborhoodDatabase;
	neighborhoodDatabase.modelDatabase = &modelDatabase;

	success &= neighborhoodDatabase.load( buildPath( distance, databaseName ) );

	if( !success ) {
		std::cout << "Failed to load the modelDatabase or the neighborhoodDatabase!\n";
		return;
	}

	Validation::NeighborhoodData validationData = Validation::NeighborhoodData::load( buildPath( distance, validationDataName ) );
	UniformWeight_ExecutionKernel executionKernel;

	const float frequencyRankExpectation = validationData.instanceCounts.calculateRankExpectation();
	log( boost::format( "control rank expectation: %i" ) % frequencyRankExpectation );

	log( boost::format( "num queries: %i" ) % validationData.queryDatasets.size() );

	const float tolerance = validationData.settings.positionVariance + 0.25;

	int rankSum = 0;

	omp_set_num_threads( 60 );

#pragma omp parallel for reduction(+ : rankSum)
	for( int sampleIndex = 0 ; sampleIndex <  validationData.queryDatasets.size() ; ++sampleIndex ) {
		std::cout << "*";
		const int modelIndex = validationData.queryInfos[ sampleIndex ];

		const auto &queryData = validationData.queryDatasets[ sampleIndex ];
		const auto results = executionKernel.execute( neighborhoodDatabase, tolerance, queryData );
		for( int resultIndex = 0 ; resultIndex < results.size() ; ++resultIndex ) {
			if( results[ resultIndex ].second == modelIndex ) {
				rankSum += resultIndex;
				break;
			}
		}
	}

	std::cout << "\n";
	const float rankExpectation = double( rankSum ) /  validationData.queryDatasets.size();

	log( boost::format(
		"%s[max distance %f, tolerance %f, position variance %f, num samples %i] // rankExpectation = %f" )
		% executionKernel.getInfoString()
		% validationData.settings.maxDistance
		% tolerance
		% validationData.settings.positionVariance
		% validationData.settings.numSamples
	 	% rankExpectation
	 );
}