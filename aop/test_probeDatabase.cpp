#include "probeDatabase.h"
#include "gtest.h"

using namespace ProbeContext;

TEST( ColorCounter, constructor ) {
	ColorCounter colorCounter;
	EXPECT_EQ( 0, colorCounter.totalNumSamples );
	for( int i = 0 ; i < colorCounter.numBuckets ; ++i ) {
		EXPECT_EQ( 0, colorCounter.buckets[ i ] );
	}
}

TEST( ColorCounter, getBucket ) {
	RawProbeSample rawProbeSample;
	rawProbeSample.colorLab.x = 0;
	rawProbeSample.colorLab.y = -100;
	rawProbeSample.colorLab.z = -100;

	EXPECT_EQ( 0, ColorCounter::getBucketIndex( rawProbeSample ) );
}

TEST( DBProbeSample, lexicographicalLess ) {
	DBProbeSample a, b;
	{
		a.occlusion = 10;
		b.occlusion = 20;

		ASSERT_TRUE( DBProbeSample::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess( b, a ) );
	}
	{
		a.occlusion = b.occlusion = 10;

		a.distance = 10;
		b.distance = 20;

		ASSERT_TRUE( DBProbeSample::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess( b, a ) );

		ASSERT_TRUE( DBProbeSample::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.occlusion = b.occlusion = 10;

		a.distance = b.distance = 10;
		a.colorLab.x = 10;
		b.colorLab.x = 20;

		ASSERT_TRUE( DBProbeSample::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess( b, a ) );

		ASSERT_TRUE( DBProbeSample::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.occlusion = b.occlusion = 10;

		a.distance = b.distance = 10;
		a.colorLab.x = a.colorLab.x = 10;

		a.colorLab.y = 10;
		b.colorLab.y = 20;

		ASSERT_TRUE( DBProbeSample::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess( b, a ) );

		ASSERT_TRUE( DBProbeSample::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.occlusion = b.occlusion = 10;

		a.distance = b.distance = 10;
		a.colorLab.x = a.colorLab.x = 10;
		a.colorLab.y = a.colorLab.y = 10;

		a.colorLab.z = 10;
		b.colorLab.z = 20;

		ASSERT_TRUE( DBProbeSample::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess( b, a ) );

		ASSERT_TRUE( DBProbeSample::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( DBProbeSample::lexicographicalLess_startWithDistance( b, a ) );
	}
}

// ProbeDataset is covered by sort_permute_iter's tests
DBProbeSample makeProbeSample( int occlusion, float distance = 10 ) {
	DBProbeSample probeSample;
	probeSample.occlusion = occlusion;
	probeSample.distance = distance;
	probeSample.colorLab.x = probeSample.colorLab.y = probeSample.colorLab.z = 0;
	return probeSample;
}

TEST( ProbeDatabase_transformSamples, idAndWeight ) {
	RawProbeSamples rawProbeSamples;

	const int minOcclusion = 3;
	const int maxOcclusion = OptixProgramInterface::numProbeSamples - 3;

	const int bucketSize = 5;

	for( int i = minOcclusion ; i <= maxOcclusion ; i++ ) {
		for( int j = 0 ; j < bucketSize ; j++ ) {
			rawProbeSamples.push_back( makeProbeSample( i, j ) );
		}
	}

	auto probeSamples = ProbeSampleTransformation::transformSamples( rawProbeSamples );

	ASSERT_EQ( rawProbeSamples.size(), probeSamples.size() );
	for( int i = 0 ; i < probeSamples.size() ; i++ ) {
		const auto &probeSample = probeSamples[ i ];
		ASSERT_EQ( i, probeSample.probeIndex );
		ASSERT_EQ( 1, probeSample.weight );
	}
}

TEST( IndexedProbeSamples, setOcclusionLowerBounds ) {
	RawProbeSamples rawProbeSamples;

	const int minOcclusion = 3;
	const int maxOcclusion = OptixProgramInterface::numProbeSamples - 3;

	const int bucketSize = 5;

	for( int i = minOcclusion ; i <= maxOcclusion ; i++ ) {
		for( int j = bucketSize - 1 ; j >= 0 ; j-- ) {
			rawProbeSamples.push_back( makeProbeSample( i, j ) );
		}
	}
	IndexedProbeSamples dataset( ProbeSampleTransformation::transformSamples( rawProbeSamples ) );

	ASSERT_EQ( rawProbeSamples.size(), dataset.size() );

	ASSERT_EQ( dataset.occlusionLowerBounds.size(), OptixProgramInterface::numProbeSamples + 2 );
	for( int i = 0 ; i < minOcclusion ; i++ ) {
		EXPECT_EQ( dataset.occlusionLowerBounds[i], 0 );
	}
	int lowerBound = 0;
	for( int i = minOcclusion ; i <= maxOcclusion; i++, lowerBound += bucketSize ) {
		EXPECT_EQ( dataset.occlusionLowerBounds[i], lowerBound );
	}
	for( int i = maxOcclusion + 1 ; i <= OptixProgramInterface::numProbeSamples + 1 ; ++i ) {
		EXPECT_EQ( dataset.occlusionLowerBounds[i], dataset.size() );
	}
}
#if 0
TEST( InstanceProbeDataset, subSet ) {
	OptixProbeSamples rawProbeSamples;

	for( int i = 0 ; i < 1000 ; i++ ) {
		rawProbeSamples.push_back( makeProbeSample( 1, 2 * i ) );
	}
	for( int i = 0 ; i < 1000 ; i++ ) {
		rawProbeSamples.push_back( makeProbeSample( 0, 2*i + 1 ) );
	}

	InstanceProbeDataset dataset = InstanceProbeDataset( rawProbeSamples );

	InstanceProbeDataset scratch = dataset.subSet( std::make_pair( 0, 2000 ) );

	for( int j = 0 ; j < 2000 ; j++ ) {
		ASSERT_EQ( j, scratch.getProbeSamples()[j].distance );
	}
}

TEST( InstanceProbeDataset, merge ) {
	OptixProbeSamples first, second;

	for( int i = 0 ; i < 1000 ; i++ ) {
		first.push_back( makeProbeSample( 0, 2*i ) );
		second.push_back( makeProbeSample( 0, 2*i + 1 ) );
	}

	InstanceProbeDataset result = InstanceProbeDataset::merge( InstanceProbeDataset( first ), InstanceProbeDataset( second ) );

	for( int j = 0 ; j < 2000 ; j++ ) {
		ASSERT_EQ( j, result.getProbeSamples()[j].distance );
	}
}

TEST( IndexedProbeSamples, mergeMultiple ) {
	const int numDatasets = 10;
	InstanceProbeDataset datasets[numDatasets];

	for( int j = 0 ; j < numDatasets ; j++ ) {
		OptixProbeSamples rawProbeSamples;
		for( int i = 0 ; i < 1000 ; i++ ) {
			rawProbeSamples.push_back( makeProbeSample( 0, numDatasets * i + j ) );
		}
		datasets[j] = InstanceProbeDataset( rawProbeSamples );
	}

	std::vector< const InstanceProbeDataset * > pDatasets;
	for( int j = 0 ; j < numDatasets ; j++ ) {
		pDatasets.push_back( &datasets[j] );
	}

	InstanceProbeDataset result = InstanceProbeDataset::mergeMultiple( pDatasets );

	for( int j = 0 ; j < numDatasets * 1000 ; j++ ) {
		ASSERT_EQ( j, result.getProbeSamples()[j].distance );
	}
}

TEST( IndexedProbeSamples, mergeMultiple_empty ) {
	const int numDatasets = 10;
	InstanceProbeDataset datasets[numDatasets];

	std::vector< const InstanceProbeDataset * > pDatasets;
	for( int j = 0 ; j < numDatasets ; j++ ) {
		pDatasets.push_back( &datasets[j] );
	}

	InstanceProbeDataset result = InstanceProbeDataset::mergeMultiple( pDatasets );

	ASSERT_EQ( result.size(), 0 );
}
#endif

TEST( ProbeDatabase, zeroTolerance ) {
	// init the dataset
	RawProbeSamples rawProbeSamples, rawTestProbeSamples;
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawProbeSamples.push_back( makeProbeSample( j, i ) );
			rawTestProbeSamples.push_back( makeProbeSample( j, 500 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( 5*1000 );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb::Transformation(), 1.0, probes, rawProbeSamples );
	probeDatabase.compileAll( 5.0 );

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeSamples );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 5000 );
		EXPECT_FLOAT_EQ( detailedQueryResults[0].probeMatchPercentage, 1.0);
		EXPECT_FLOAT_EQ( detailedQueryResults[0].queryMatchPercentage, 1.0);
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );

		auto queryResults = query.getQueryResults();

		ASSERT_EQ( queryResults.size(), 1 );
		EXPECT_FLOAT_EQ( queryResults[0].score, 1.0 );
		EXPECT_EQ( queryResults[0].sceneModelIndex, 0 );
	}

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawTestProbeSamples );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 2500 );
		EXPECT_FLOAT_EQ( detailedQueryResults[0].probeMatchPercentage, 0.5);
		EXPECT_FLOAT_EQ( detailedQueryResults[0].queryMatchPercentage, 0.5);
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );

		auto queryResults = query.getQueryResults();

		ASSERT_EQ( queryResults.size(), 1 );
		EXPECT_FLOAT_EQ( queryResults[0].score, 0.25 );
		EXPECT_EQ( queryResults[0].sceneModelIndex, 0 );
	}
}

TEST( ProbeDatabase, zeroTolerance_biggerDB ) {
	// init the dataset
	RawProbeSamples rawProbeSamples, rawTestProbeSamples;
	for( int i = 0 ; i < 1500 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawProbeSamples.push_back( makeProbeSample( j, i ) );
		}
	}
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawTestProbeSamples.push_back( makeProbeSample( j, 1000 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( 5*1500 );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb::Transformation(), 1.0, probes, rawProbeSamples );
	probeDatabase.compileAll( 5.0 );

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeSamples );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( 1, detailedQueryResults.size() );
		EXPECT_EQ( 5*1500, detailedQueryResults[0].numMatches );
		EXPECT_FLOAT_EQ( 1.0, detailedQueryResults[0].probeMatchPercentage );
		EXPECT_FLOAT_EQ( 1.0, detailedQueryResults[0].queryMatchPercentage );
		EXPECT_EQ( 0, detailedQueryResults[0].sceneModelIndex );

		auto queryResults = query.getQueryResults();

		ASSERT_EQ( queryResults.size(), 1 );
		EXPECT_FLOAT_EQ( queryResults[0].score, 1.0 );
		EXPECT_EQ( queryResults[0].sceneModelIndex, 0 );
	}

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawTestProbeSamples );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 5*500 );
		EXPECT_FLOAT_EQ( detailedQueryResults[0].probeMatchPercentage, 500.0/1500.0);
		EXPECT_FLOAT_EQ( detailedQueryResults[0].queryMatchPercentage, 500.0/1000.0);
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );

		auto queryResults = query.getQueryResults();

		ASSERT_EQ( queryResults.size(), 1 );
		EXPECT_FLOAT_EQ( queryResults[0].score, 500.0/1500.0 * 500.0/1000.0 );
		EXPECT_EQ( queryResults[0].sceneModelIndex, 0 );
	}
}

TEST( ProbeDatabase, oneTolerance ) {
	// init the dataset
	RawProbeSamples rawProbeSamples, rawTestProbeSamples;
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawProbeSamples.push_back( makeProbeSample( j, i ) );
			rawTestProbeSamples.push_back( makeProbeSample( j, 500 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( 5*1000 );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb::Transformation(), 1.0, probes, rawProbeSamples );
	probeDatabase.compileAll( 5.0 );

	ProbeContextTolerance pct;
	pct.occusionTolerance = 1.0 / (OptixProgramInterface::numProbeSamples + 1);
	pct.distanceTolerance = 0;

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeSamples );
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 2*2*1000 + 3*3*1000 );
		EXPECT_FLOAT_EQ( detailedQueryResults[0].probeMatchPercentage, 1.0);
		EXPECT_FLOAT_EQ( detailedQueryResults[0].queryMatchPercentage, 1.0);
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );

		auto queryResults = query.getQueryResults();

		ASSERT_EQ( queryResults.size(), 1 );
		EXPECT_FLOAT_EQ( queryResults[0].score, 1.0 );
	}

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawTestProbeSamples );
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 2*2*500 + 3*3*500 );
		EXPECT_FLOAT_EQ( detailedQueryResults[0].probeMatchPercentage, 0.5);
		EXPECT_FLOAT_EQ( detailedQueryResults[0].queryMatchPercentage, 0.5);
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );

		auto queryResults = query.getQueryResults();

		ASSERT_EQ( queryResults.size(), 1 );
		EXPECT_FLOAT_EQ( queryResults[0].score, 0.25 );
		EXPECT_EQ( queryResults[0].sceneModelIndex, 0 );
	}
}

#if 1
TEST( ProbeDatabase, big ) {
	// init the dataset
	RawProbeSamples rawProbeSamples, rawTestProbeSamples;
	for( int i = 0 ; i < 20000 ; i++ ) {
		for( int j = 0 ; j < 30 ; j++ ) {
			rawProbeSamples.push_back( makeProbeSample( j, i ) );
			rawTestProbeSamples.push_back( makeProbeSample( j, 10000 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( rawProbeSamples.size() );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb::Transformation(), 1.0, probes, rawProbeSamples );
	probeDatabase.compileAll( 5.0 );

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeSamples );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 600000 );
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );
	}
	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawTestProbeSamples );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 300000 );
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );
	}
}


TEST( ProbeDatabase, bigWithTolerance ) {
	// init the dataset
	RawProbeSamples rawProbeSamples, rawTestProbeSamples;
	for( int i = 0 ; i < 20000 ; i++ ) {
		for( int j = 0 ; j < 30 ; j++ ) {
			rawProbeSamples.push_back( makeProbeSample( j, i ) );
			rawTestProbeSamples.push_back( makeProbeSample( j, 10000 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( rawProbeSamples.size() );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb::Transformation(), 1.0, probes, rawProbeSamples );
	probeDatabase.compileAll( 5.0 );

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeSamples );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 2;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 600000 );
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );
	}
	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawTestProbeSamples );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 2;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::DetailedQueryResults detailedQueryResults = query.getDetailedQueryResults();

		ASSERT_EQ( detailedQueryResults.size(), 1 );
		EXPECT_EQ( detailedQueryResults[0].numMatches, 300000 );
		EXPECT_EQ( detailedQueryResults[0].sceneModelIndex, 0 );
	}
}
#endif
