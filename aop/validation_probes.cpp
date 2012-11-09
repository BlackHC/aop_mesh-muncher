#include "modelDatabase.h"
#include "probeDatabase.h"
#include "probeGenerator.h"

#include "validation.h"

#include <string>
#include <boost/format.hpp>

using namespace ProbeContext;

const std::string basePath = "probeValidationData";
const std::string modelDatabaseName = "modelDatabase";
const std::string databaseName = "probeDatabase";
const std::string validationDataName = "probe.validationData";

std::string buildPath( const std::string &testCase, const std::string &filename ) {
	const std::string path = boost::str(
		boost::format( "%s/%s/%s")
		% basePath
		% testCase
		% filename
	);
	return path;
}

ProbeContextTolerance createFromSettings( const Validation::ProbeSettings &settings ) {
	ProbeContextTolerance pct;
	pct.colorLabTolerance = settings.colorTolerance;
	pct.distanceTolerance = settings.distanceTolerance;
	pct.occusionTolerance = settings.occlusionTolerance;
	return pct;
}

struct UniformBidirectional_ExecutionKernel {
	static QueryResults execute( const ProbeDatabase &probeDatabase, const Validation::ProbeSettings &settings, const Validation::ProbeData::QueryData &queryData ) {
		ProbeContext::ProbeDatabase::Query query( probeDatabase );
		{
			query.setQueryDataset( queryData.querySamples );

			query.setProbeContextTolerance( createFromSettings( settings ) );

			query.execute();
		}
		return query.getQueryResults();
	}

	static std::string getInfoString() {
		return "Simple Query";
	}
};

struct UniformFull_ExecutionKernel {
	static QueryResults execute( const ProbeDatabase &probeDatabase, const Validation::ProbeSettings &settings, const Validation::ProbeData::QueryData &queryData ) {
		ProbeContext::ProbeDatabase::FullQuery query( probeDatabase );
		{
			query.setQueryVolume( queryData.queryVolume, settings.resolution );
			query.setQueryDataset( queryData.queryProbes, queryData.querySamples );

			query.setProbeContextTolerance( createFromSettings( settings ) );

			query.execute();
		}
		return query.getQueryResults();
	}

	static std::string getInfoString() {
		return "Simple Query";
	}
};

void main() {
	const std::string testCase = "simple";

	ProbeGenerator::initDirections();
	ProbeGenerator::initOrientations();

	bool success = true;

	ModelDatabase modelDatabase( nullptr );
	success &= modelDatabase.load( buildPath( testCase, modelDatabaseName ) );

	ProbeContext::ProbeDatabase probeDatabase;

	success &= probeDatabase.load( buildPath( testCase, databaseName ) );

	if( !success ) {
		std::cout << "Failed to load the model database or the probe database!\n";
		return;
	}

	Validation::ProbeData validationData = Validation::ProbeData::load( buildPath( testCase, validationDataName ) );

	probeDatabase.registerSceneModels( validationData.localModelNames );

	UniformFull_ExecutionKernel executionKernel;

	const float frequencyRankExpectation = validationData.instanceCounts.calculateRankExpectation();
	log( boost::format( "control rank expectation: %i" ) % frequencyRankExpectation );

	log( boost::format( "num queries: %i" ) % validationData.queries.size() );

	int rankSum = 0;

	if( probeDatabase.getNumSampledModels() != validationData.instanceCounts.instanceCounts.size() ) {
		logError( "probeDatabase.getNumSampledModels() != validationData.instanceCounts.instanceCounts.size() --- wrong results can be expected!" );
	}

	omp_set_num_threads( 10 );

	#pragma omp parallel for reduction(+ : rankSum)
	for( int sampleIndex = 0 ; sampleIndex <  validationData.queries.size() ; ++sampleIndex ) {
		std::cout << "*";
		const auto &queryData = validationData.queries[ sampleIndex ];
		const int sceneModelIndex = queryData.expectedSceneModelIndex;;

		auto queryResults = executionKernel.execute( probeDatabase, validationData.settings, queryData );
		boost::sort(
			queryResults,
			QueryResult::greaterByScoreAndModelIndex
		);

		for( int resultIndex = 0 ; resultIndex < queryResults.size() ; resultIndex++, rankSum++ ) {
			if( queryResults[ resultIndex ].sceneModelIndex == sceneModelIndex ) {
				break;
			}
		}
	}

	std::cout << "\n";

	const float rankExpectation = double( rankSum ) /  validationData.queries.size();

	log( boost::format(
		"%s[max distance -> %f, resolution -> %f, tolerance -> {%f, %f, %f}, position variance -> %f, num samples -> %i, query volume size -> %f] // rankExpectation = %f" )
		% executionKernel.getInfoString()
		% validationData.settings.maxDistance
		% validationData.settings.resolution
		% validationData.settings.occlusionTolerance
		% validationData.settings.distanceTolerance
		% validationData.settings.colorTolerance
		% validationData.settings.positionVariance
		% validationData.settings.numSamples
		% validationData.settings.queryVolumeSize
	 	% rankExpectation
	 );
}