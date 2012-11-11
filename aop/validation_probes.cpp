#include "validationStorage.h"

#include "modelDatabase.h"
#include "probeDatabase.h"
#include "probeGenerator.h"

#include <string>
#include <boost/format.hpp>

using namespace ProbeContext;

const std::string defaultConfigName = "probeValidation.wml";

struct Config {
	// base names for dirs
	std::string base;
	// base + / + results
	std::string results;

	// filenames
	std::string modelDatabase;
	std::string probeDatabase;
	std::string validationData;

	// basic config
	int sampleSelector;

	// work splitting on a cluster
	int numMachines;
	int machineIndex;
	int numCores;

	// runs
	typedef std::vector< std::string > Jobs;
	typedef std::pair< std::string, Jobs > VarianceJobs;
	typedef std::vector< VarianceJobs > DistanceVarianceJobs;
	DistanceVarianceJobs jobs;

	Config()
		: sampleSelector()
		, base( "C:/Andreas/probeValidationData/road" )
		, results( "results" )
		, modelDatabase( "modelDatabase" )
		, probeDatabase( "probeDatabase" )
		, validationData( "probe.validationData" )
		, numMachines(1)
		, machineIndex()
		, numCores(1)
	{
	}

	SERIALIZER_DEFAULT_IMPL(
		(sampleSelector)
		(base)
		(results)
		(modelDatabase)
		(probeDatabase)
		(validationData)
		(jobs)
		(numMachines)
		(machineIndex)
		(numCores)
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

	std::string buildProbeDatabasePath( const std::string &distancePath ) const {
		return buildPath( distancePath + "/" + probeDatabase );
	}

	std::string buildValidationDataPath( const std::string &distancePath, const std::string &variancePath ) const {
		return buildPath( distancePath + "/" + variancePath + "/" + validationData );
	}
};

Config config;

//////////////////////////////////////////////////////////////////////////
typedef std::vector< std::pair< int, Neighborhood::Results > > FullResults;
typedef std::vector< int > RankResults;

// per validation data
template< typename Result >
struct KernelResults {
	Result uniformBidirectional, importanceBidirectional, uniformConfiguration, importanceConfiguration,
		fastUniformBidirectional, fastUniformConfiguration;

	SERIALIZER_DEFAULT_IMPL(
		(uniformBidirectional)
		(importanceBidirectional)
		(uniformConfiguration)
		(importanceConfiguration)
		(fastUniformBidirectional)
		(fastUniformConfiguration)
	)

	KernelResults()
		: uniformBidirectional()
		, importanceBidirectional()
		, uniformConfiguration()
		, importanceConfiguration()
		, fastUniformBidirectional()
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

SERIALIZER_DEFAULT_EXTERN_IMPL( boost::timer::cpu_times,
	(wall)
	(user)
	(system)
)

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

ProbeContextTolerance createFromSettings( const Validation::ProbeSettings &settings ) {
	ProbeContextTolerance pct;
	pct.colorLabTolerance = settings.colorTolerance;
	pct.distanceTolerance = settings.distanceTolerance;
	pct.occusionTolerance = settings.occlusionTolerance;
	return pct;
}

struct FastUniformBidirectional_ExecutionKernel {
	static QueryResults execute( const ProbeDatabase &probeDatabase, const Validation::ProbeSettings &settings, const Validation::ProbeData::QueryData &queryData ) {
		ProbeContext::ProbeDatabase::FastQuery query( probeDatabase );
		{
			query.setQueryDataset( queryData.querySamples );
			query.setQueryVolume( queryData.queryVolume, settings.resolution );

			query.setProbeContextTolerance( createFromSettings( settings ) );

			query.execute();
		}
		return query.getQueryResults();
	}

	static std::string getInfoString() {
		return "FastUniformBidirectional";
	}
};

struct UniformBidirectional_ExecutionKernel {
	static QueryResults execute( const ProbeDatabase &probeDatabase, const Validation::ProbeSettings &settings, const Validation::ProbeData::QueryData &queryData ) {
		ProbeContext::ProbeDatabase::Query query( probeDatabase );
		{
			query.setQueryDataset( queryData.querySamples );
			query.setQueryVolume( queryData.queryVolume, settings.resolution );

			query.setProbeContextTolerance( createFromSettings( settings ) );

			query.execute();
		}
		return query.getQueryResults();
	}

	static std::string getInfoString() {
		return "Simple Query";
	}
};

struct FastUniformFull_ExecutionKernel {
	static QueryResults execute( const ProbeDatabase &probeDatabase, const Validation::ProbeSettings &settings, const Validation::ProbeData::QueryData &queryData ) {
		ProbeContext::ProbeDatabase::FastConfigurationQuery query( probeDatabase );
		{
			query.setQueryVolume( queryData.queryVolume, settings.resolution );
			query.setQueryDataset( queryData.queryProbes, queryData.querySamples );

			query.setProbeContextTolerance( createFromSettings( settings ) );

			query.execute();
		}
		return query.getQueryResults();
	}

	static std::string getInfoString() {
		return "FastConfigurationQuery";
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

struct ImportanceBidirectional_ExecutionKernel {
	static QueryResults execute( const ProbeDatabase &probeDatabase, const Validation::ProbeSettings &settings, const Validation::ProbeData::QueryData &queryData ) {
		ProbeContext::ProbeDatabase::ImportanceQuery query( probeDatabase );
		{
			query.setQueryDataset( queryData.querySamples );
			query.setQueryVolume( queryData.queryVolume, settings.resolution );

			query.setProbeContextTolerance( createFromSettings( settings ) );

			query.execute();
		}
		return query.getQueryResults();
	}

	static std::string getInfoString() {
		return "Importance Simple Query";
	}
};

struct ImportanceFull_ExecutionKernel {
	static QueryResults execute( const ProbeDatabase &probeDatabase, const Validation::ProbeSettings &settings, const Validation::ProbeData::QueryData &queryData ) {
		ProbeContext::ProbeDatabase::ImportanceFullQuery query( probeDatabase );
		{
			query.setQueryVolume( queryData.queryVolume, settings.resolution );
			query.setQueryDataset( queryData.queryProbes, queryData.querySamples );

			query.setProbeContextTolerance( createFromSettings( settings ) );

			query.execute();
		}
		return query.getQueryResults();
	}

	static std::string getInfoString() {
		return "Importance Full Query";
	}
};

template<typename ExecutionKernel>
void executeKernel(
	const ProbeContext::ProbeDatabase &probeDatabase,
	const Validation::ProbeData &validationData,

	int beginSampleIndex,
	int endSampleIndex,

	float &expectationResult,
	TimerResults &timerResults,
	RankResults &rankResults,
	FullResults &unsortedResults
) {
	// misnomers: numTotalSamples should be numInstances and numSamples numSamplesPerInstance!
	const int numSamples = validationData.settings.numSamples;
	const int numTotalSamples = endSampleIndex - beginSampleIndex;

	rankResults.resize( numTotalSamples );
	unsortedResults.resize( numTotalSamples );

	int rankSum = 0;

	boost::timer::cpu_timer cpuTimer;

#pragma omp parallel for reduction(+ : rankSum)
	for( int sampleIndex = beginSampleIndex ; sampleIndex < endSampleIndex ; sampleIndex++  ) {
		const int outputIndex = sampleIndex - beginSampleIndex;
		const int queryIndex = sampleIndex * numSamples + config.sampleSelector;

		const auto &queryData = validationData.queries[ sampleIndex ];
		const int sceneModelIndex = queryData.expectedSceneModelIndex;

		QueryResults queryResults = ExecutionKernel::execute( probeDatabase, validationData.settings, queryData );
		//QueryResults queryResults;

		{
			unsortedResults[ outputIndex ].first = sceneModelIndex;
			Neighborhood::Results &simpleResults = unsortedResults[ outputIndex ].second;
			simpleResults.reserve( queryResults.size() );
			for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
				simpleResults.push_back( std::make_pair( queryResult->score, queryResult->sceneModelIndex ) );
			}
		}

		boost::sort(
			queryResults,
			QueryResult::greaterByScoreAndModelIndex
		);

		rankResults[ outputIndex ] = queryResults.size();
		for( int resultIndex = 0 ; resultIndex < queryResults.size() ; ++resultIndex ) {
			if( queryResults[ resultIndex ].sceneModelIndex == sceneModelIndex ) {
				rankResults[ outputIndex ] = resultIndex;
				rankSum += resultIndex;
				break;
			}
		}

		if( (outputIndex % 2) == 0 ) {
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
	ProbeContext::ProbeDatabase &probeDatabase,
	const std::string &validationDataFilePath,
	ExpectationsResults &expectationsResults
) {
	Validation::ProbeData validationData;

	{
		bool success = Validation::ProbeData::load( validationDataFilePath, validationData );

		if( !success ) {
			logError( boost::format( "Failed to load validationData '%s'!\n" ) % validationDataFilePath );
			return;
		}
	}

	// make sure we use the correct model index map
	probeDatabase.registerSceneModels( validationData.localModelNames );

	ExpectationsResult expectationResult;
	ValidationDataResults<RankResults> validationDataRanks;
	ValidationDataResults<FullResults> fullResults;

	expectationResult.settings = validationData.settings;
	fullResults.settings = validationDataRanks.settings = validationData.settings;

	expectationResult.info = ValidatorInfo::globalInfo;
	fullResults.info = validationDataRanks.info = ValidatorInfo::globalInfo;

	const float maxFrequency_expectedRank = validationData.instanceCounts.calculateRankExpectation();
	expectationResult.maxFrequency_expectedRank = maxFrequency_expectedRank;
	fullResults.maxFrequency_expectedRank = validationDataRanks.maxFrequency_expectedRank = maxFrequency_expectedRank;

	if( config.sampleSelector < validationData.settings.numSamples ) {
		expectationResult.numQueries = validationData.queries.size() / validationData.settings.numSamples;

		log(
			boost::format(
				"max distance -> %f\n"
				"resolution -> %f\n"
				"tolerance -> {%f, %f, %f}\n"
				"position variance -> %f\n"
				"num samples -> %i\n"
				"query volume size -> %f"
			)
			% validationData.settings.maxDistance
			% validationData.settings.resolution
			% validationData.settings.occlusionTolerance
			% validationData.settings.distanceTolerance
			% validationData.settings.colorTolerance
			% validationData.settings.positionVariance
			% validationData.settings.numSamples
			% validationData.settings.queryVolumeSize
		);

		log( boost::format( "maxFrequency // rank expectation: %f" ) % maxFrequency_expectedRank );

		// determine the sample range
		const int numSamples = validationData.settings.numSamples;
		const int numInstances = validationData.queries.size() / numSamples;
		// split onto machines and round up
		const int averageNumInstancesPerMachine = (numInstances + config.numMachines - 1)/ config.numMachines;
		const int beginIndex = averageNumInstancesPerMachine * config.machineIndex;
		if( beginIndex >= numInstances )  {
			log( "empty sample range for this machine!" );
			return;
		}
		const int endIndex = std::min( beginIndex + averageNumInstancesPerMachine * config.numCores, numInstances );

		log(
			boost::format( "num instances for this machine: %i (total %i) [%i--%i) for %i cores" )
			% (endIndex - beginIndex)
			% numInstances
			% beginIndex
			% endIndex
			% config.numCores
		);

		log( "FastUniformBidirectional" );
		executeKernel<FastUniformBidirectional_ExecutionKernel>(
			probeDatabase,
			validationData,

			beginIndex,
			endIndex,

			expectationResult.expectedRanks.fastUniformBidirectional,
			expectationResult.timers.fastUniformBidirectional,
			validationDataRanks.results.fastUniformBidirectional,
			fullResults.results.fastUniformBidirectional
		);

		log( "FastConfigurationQuery" );
		executeKernel<FastUniformFull_ExecutionKernel>(
			probeDatabase,
			validationData,

			beginIndex,
			endIndex,

			expectationResult.expectedRanks.fastUniformConfiguration,
			expectationResult.timers.fastUniformConfiguration,
			validationDataRanks.results.fastUniformConfiguration,
			fullResults.results.fastUniformConfiguration
		);

		/*log( "UniformBidirectional" );
		executeKernel<UniformBidirectional_ExecutionKernel>(
			probeDatabase,
			validationData,

			beginIndex,
			endIndex,

			expectationResult.expectedRanks.uniformBidirectional,
			expectationResult.timers.uniformBidirectional,
			validationDataRanks.results.uniformBidirectional,
			fullResults.results.uniformBidirectional
		);

		log( "UniformConfiguration" );
		executeKernel<UniformFull_ExecutionKernel>(
			probeDatabase,
			validationData,

			beginIndex,
			endIndex,

			expectationResult.expectedRanks.uniformConfiguration,
			expectationResult.timers.uniformConfiguration,
			validationDataRanks.results.uniformConfiguration,
			fullResults.results.uniformConfiguration
		);

		log( "ImportanceBidirectional" );
		executeKernel<ImportanceBidirectional_ExecutionKernel>(
			probeDatabase,
			validationData,

			beginIndex,
			endIndex,

			expectationResult.expectedRanks.importanceBidirectional,
			expectationResult.timers.importanceBidirectional,
			validationDataRanks.results.importanceBidirectional,
			fullResults.results.importanceBidirectional
		);

		log( "ImportanceConfiguration" );
		executeKernel<ImportanceFull_ExecutionKernel>(
			probeDatabase,
			validationData,

			beginIndex,
			endIndex,

			expectationResult.expectedRanks.importanceConfiguration,
			expectationResult.timers.importanceConfiguration,
			validationDataRanks.results.importanceConfiguration,
			fullResults.results.importanceConfiguration
		);*/

		{
			const std::string resultFilePath = config.buildResultPath(
				boost::str( boost::format( "%i__%i_%i_%i.probe.rankResults.wml" )
					% config.sampleSelector
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
				boost::str( boost::format( "%i__%i_%i_%i.probe.fullResults.wml" )
					% config.sampleSelector
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
			boost::str( boost::format( "%i.probe.expectationsResults.wml" )
				% config.sampleSelector
			)
		);

		Serializer::TextWriter writer( resultFilePath );
		Serializer::write( writer, expectationsResults );
	}
}

void testProbeDatabase(
	ModelDatabase &modelDatabase,
	const Config::VarianceJobs &jobs,
	ExpectationsResults &expectationsResults
) {
	ProbeContext::ProbeDatabase probeDatabase;

	{
		const std::string path = config.buildProbeDatabasePath( jobs.first );
		bool success = probeDatabase.load( path );

		if( !success ) {
			logError( boost::format( "Failed to load probe database '%s'!\n" ) % path );
			return;
		}
	}

	for( auto job = jobs.second.begin() ; job != jobs.second.end() ; ++job ) {
		const std::string path = config.buildValidationDataPath( jobs.first, *job );
		testValidationData( probeDatabase, path, expectationsResults );
	}
}


void real_main( int argc, const char **argv ) {
	omp_set_num_threads( 6 );

	ProbeGenerator::initDirections();
	ProbeGenerator::initOrientations();

	// load config
	std::string configName = defaultConfigName;
	if( argc == 2 ) {
		configName = argv[1];
	}

	{
		Serializer::TextWriter writer( "probeValidation_defaultConfig.wml" );
		Serializer::write( writer, config );
	}

	{
		Serializer::TextReader reader( configName );
		log( "loaded config" );
		log( wml::emit( reader.root ) );
		Serializer::read( reader, config );
	}

	ValidatorInfo::globalInfo.machineIndex = config.machineIndex;
	ValidatorInfo::globalInfo.numMachines = config.numMachines;
	ValidatorInfo::globalInfo.numCores = config.numCores;

	ModelDatabase modelDatabase( nullptr );
	{
		bool success = modelDatabase.load( config.buildModelDatabasePath() );

		if( !success ) {
			logError( "Failed to load the model database!\n" );
			return;
		}
	}

	ExpectationsResults expectationsResults;

	for( auto distanceJobs = config.jobs.begin() ; distanceJobs != config.jobs.end() ; ++distanceJobs ) {
		testProbeDatabase( modelDatabase, *distanceJobs, expectationsResults );
	}
}

void main( int argc, const char **argv ) {
	/*MEMORYSTATUSEX memoryStatusEx;
	memoryStatusEx.dwLength = sizeof( memoryStatusEx );
	GlobalMemoryStatusEx( &memoryStatusEx );

	std::cout
		<< "Memory info\n"
		<< "\tMemory load: " << memoryStatusEx.dwMemoryLoad << "%\n"
		<< "\tTotal physical memory: " << (memoryStatusEx.ullTotalPhys >> 30) << " GB\n"
		<< "\tAvail physical memory: " << (memoryStatusEx.ullAvailPhys >> 30) << " GB\n";

	const int maxLoad = 90;
	if( memoryStatusEx.dwMemoryLoad > maxLoad ) {
		std::cerr << "Memory load over " << maxLoad << "!\n";
		return;
	}

	const size_t minMemory = 500 << 20;
	const size_t saveMemory = 500 << 20;
	const size_t memoryLimit = memoryStatusEx.ullAvailPhys - saveMemory;
	if( memoryLimit < minMemory) {
		std::cerr << "Not enough memory available (" << (minMemory >> 20) << " MB)!\n";
		return;
	}

	HANDLE jobObject = CreateJobObject( NULL, NULL );

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
	info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
	info.ProcessMemoryLimit = memoryLimit;
	if( !SetInformationJobObject( jobObject, JobObjectExtendedLimitInformation, &info, sizeof( info ) ) ) {
		std::cerr << "Failed to set the memory limit!\n";
		return;
	}

	if( !AssignProcessToJobObject( jobObject, GetCurrentProcess() ) ) {
		std::cerr << "Failed to assign the process to its job object! Try to deactivate the application compatibility assistant for Visual Studio 2010!\n";
		return;
	}

	std::cout << "Memory limit set to " << (memoryLimit >> 20) << " MB\n\n";*/

	try {
		real_main( argc, argv );
	}
	catch( std::exception &e) {
		std::cout << e.what() << "\n";
	}
}