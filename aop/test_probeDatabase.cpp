#include "probeDatabase.h"
#include "gtest.h"

TEST( DBProbeContext, lexicographicalLess ) {
	DBProbeContext a, b;
	{
		a.hitCounter = 10;
		b.hitCounter = 20;

		ASSERT_TRUE( DBProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = 10;
		b.distance = 20;

		ASSERT_TRUE( DBProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess( b, a ) );

		ASSERT_TRUE( DBProbeContext::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.Lab.x = 10;
		b.Lab.x = 20;

		ASSERT_TRUE( DBProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess( b, a ) );

		ASSERT_TRUE( DBProbeContext::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.Lab.x = a.Lab.x = 10;

		a.Lab.y = 10;
		b.Lab.y = 20;

		ASSERT_TRUE( DBProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess( b, a ) );

		ASSERT_TRUE( DBProbeContext::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.Lab.x = a.Lab.x = 10;
		a.Lab.y = a.Lab.y = 10;

		a.Lab.z = 10;
		b.Lab.z = 20;

		ASSERT_TRUE( DBProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess( b, a ) );

		ASSERT_TRUE( DBProbeContext::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( DBProbeContext::lexicographicalLess_startWithDistance( b, a ) );
	}
}

// ProbeDataset is covered by sort_permute_iter's tests
DBProbeContext makeProbeContext( int hitCounter, float distance = 10 ) {
	DBProbeContext probeContext;
	probeContext.hitCounter = hitCounter;
	probeContext.distance = distance;
	probeContext.Lab.x = probeContext.Lab.y = probeContext.Lab.z = 0;
	return probeContext;
}

TEST( ProbeDatabase_transformContexts, idAndWeight ) {
	RawProbeContexts rawProbeContexts;

	const int minHitCounter = 3;
	const int maxHitCounter = OptixProgramInterface::numProbeSamples - 3;

	const int bucketSize = 5;

	for( int i = minHitCounter ; i <= maxHitCounter ; i++ ) {
		for( int j = 0 ; j < bucketSize ; j++ ) {
			rawProbeContexts.push_back( makeProbeContext( i, j ) );
		}
	}

	auto probeContexts = ProbeContextTransformation::transformContexts( rawProbeContexts );

	ASSERT_EQ( rawProbeContexts.size(), probeContexts.size() );
	for( int i = 0 ; i < probeContexts.size() ; i++ ) {
		const auto &probeContext = probeContexts[ i ];
		ASSERT_EQ( i, probeContext.probeIndex );
		ASSERT_EQ( 1, probeContext.weight );
	}
}

TEST( IndexedProbeContexts, setHitCounterLowerBounds ) {
	RawProbeContexts rawProbeContexts;

	const int minHitCounter = 3;
	const int maxHitCounter = OptixProgramInterface::numProbeSamples - 3;

	const int bucketSize = 5;

	for( int i = minHitCounter ; i <= maxHitCounter ; i++ ) {
		for( int j = bucketSize - 1 ; j >= 0 ; j-- ) {
			rawProbeContexts.push_back( makeProbeContext( i, j ) );
		}
	}
	IndexedProbeContexts dataset( ProbeContextTransformation::transformContexts( rawProbeContexts ) );

	ASSERT_EQ( rawProbeContexts.size(), dataset.size() );

	ASSERT_EQ( dataset.hitCounterLowerBounds.size(), OptixProgramInterface::numProbeSamples + 2 );
	for( int i = 0 ; i < minHitCounter ; i++ ) {
		EXPECT_EQ( dataset.hitCounterLowerBounds[i], 0 );
	}
	int lowerBound = 0;
	for( int i = minHitCounter ; i <= maxHitCounter; i++, lowerBound += bucketSize ) {
		EXPECT_EQ( dataset.hitCounterLowerBounds[i], lowerBound );
	}
	for( int i = maxHitCounter + 1 ; i <= OptixProgramInterface::numProbeSamples + 1 ; ++i ) {
		EXPECT_EQ( dataset.hitCounterLowerBounds[i], dataset.size() );
	}
}
#if 0
TEST( InstanceProbeDataset, subSet ) {
	OptixProbeContexts rawProbeContexts;

	for( int i = 0 ; i < 1000 ; i++ ) {
		rawProbeContexts.push_back( makeProbeContext( 1, 2 * i ) );
	}
	for( int i = 0 ; i < 1000 ; i++ ) {
		rawProbeContexts.push_back( makeProbeContext( 0, 2*i + 1 ) );
	}

	InstanceProbeDataset dataset = InstanceProbeDataset( rawProbeContexts );

	InstanceProbeDataset scratch = dataset.subSet( std::make_pair( 0, 2000 ) );

	for( int j = 0 ; j < 2000 ; j++ ) {
		ASSERT_EQ( j, scratch.getProbeContexts()[j].distance );
	}
}

TEST( InstanceProbeDataset, merge ) {
	OptixProbeContexts first, second;

	for( int i = 0 ; i < 1000 ; i++ ) {
		first.push_back( makeProbeContext( 0, 2*i ) );
		second.push_back( makeProbeContext( 0, 2*i + 1 ) );
	}

	InstanceProbeDataset result = InstanceProbeDataset::merge( InstanceProbeDataset( first ), InstanceProbeDataset( second ) );

	for( int j = 0 ; j < 2000 ; j++ ) {
		ASSERT_EQ( j, result.getProbeContexts()[j].distance );
	}
}

TEST( IndexedProbeContexts, mergeMultiple ) {
	const int numDatasets = 10;
	InstanceProbeDataset datasets[numDatasets];

	for( int j = 0 ; j < numDatasets ; j++ ) {
		OptixProbeContexts rawProbeContexts;
		for( int i = 0 ; i < 1000 ; i++ ) {
			rawProbeContexts.push_back( makeProbeContext( 0, numDatasets * i + j ) );
		}
		datasets[j] = InstanceProbeDataset( rawProbeContexts );
	}

	std::vector< const InstanceProbeDataset * > pDatasets;
	for( int j = 0 ; j < numDatasets ; j++ ) {
		pDatasets.push_back( &datasets[j] );
	}

	InstanceProbeDataset result = InstanceProbeDataset::mergeMultiple( pDatasets );

	for( int j = 0 ; j < numDatasets * 1000 ; j++ ) {
		ASSERT_EQ( j, result.getProbeContexts()[j].distance );
	}
}

TEST( IndexedProbeContexts, mergeMultiple_empty ) {
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
	RawProbeContexts rawProbeContexts, rawTestProbeContexts;
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawProbeContexts.push_back( makeProbeContext( j, i ) );
			rawTestProbeContexts.push_back( makeProbeContext( j, 500 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( 5*1000 );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb(), probes, rawProbeContexts );
	probeDatabase.compileAll();

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeContexts );

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

		query.setQueryDataset( rawTestProbeContexts );

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
	RawProbeContexts rawProbeContexts, rawTestProbeContexts;
	for( int i = 0 ; i < 1500 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawProbeContexts.push_back( makeProbeContext( j, i ) );
		}
	}
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawTestProbeContexts.push_back( makeProbeContext( j, 1000 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( 5*1500 );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb(), probes, rawProbeContexts );
	probeDatabase.compileAll();

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeContexts );

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

		query.setQueryDataset( rawTestProbeContexts );

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
	RawProbeContexts rawProbeContexts, rawTestProbeContexts;
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawProbeContexts.push_back( makeProbeContext( j, i ) );
			rawTestProbeContexts.push_back( makeProbeContext( j, 500 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( 5*1000 );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb(), probes, rawProbeContexts );
	probeDatabase.compileAll();

	ProbeContextTolerance pct;
	pct.occusionTolerance = 1.0 / (OptixProgramInterface::numProbeSamples + 1);
	pct.distanceTolerance = 0;

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeContexts );
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

		query.setQueryDataset( rawTestProbeContexts );
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

#if 0
TEST( ProbeDatabase, big ) {
	// init the dataset
	RawProbeContexts rawProbeContexts, rawTestProbeContexts;
	for( int i = 0 ; i < 20000 ; i++ ) {
		for( int j = 0 ; j < 30 ; j++ ) {
			rawProbeContexts.push_back( makeProbeContext( j, i ) );
			rawTestProbeContexts.push_back( makeProbeContext( j, 10000 + i ) );
		}
	}

	auto probes = std::vector< DBProbe >( rawProbeContexts.size() );

	ProbeDatabase probeDatabase;

	std::vector< std::string > modelNames;
	modelNames.push_back( "test" );
	probeDatabase.registerSceneModels( modelNames );

	probeDatabase.addInstanceProbes( 0, Obb(), probes, rawProbeContexts );
	probeDatabase.compileAll();

	{
		ProbeDatabase::Query query( probeDatabase );

		query.setQueryDataset( rawProbeContexts );

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

		query.setQueryDataset( rawTestProbeContexts );

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
#endif
