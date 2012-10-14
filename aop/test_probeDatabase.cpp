#include "probeDatabase.h"
#include "gtest.h"

TEST( SortedProbeDataset_ProbeContext, lexicographicalLess ) {
	SortedProbeDataset::ProbeContext a, b;
	{
		a.hitCounter = 10;
		b.hitCounter = 20;

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = 10;
		b.distance = 20;

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess( b, a ) );

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.Lab.x = 10;
		b.Lab.x = 20;

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess( b, a ) );

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.Lab.x = a.Lab.x = 10;

		a.Lab.y = 10;
		b.Lab.y = 20;

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess( b, a ) );

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.Lab.x = a.Lab.x = 10;
		a.Lab.y = a.Lab.y = 10;

		a.Lab.z = 10;
		b.Lab.z = 20;

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess( b, a ) );

		ASSERT_TRUE( SortedProbeDataset::ProbeContext::lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( SortedProbeDataset::ProbeContext::lexicographicalLess_startWithDistance( b, a ) );
	}
}

// ProbeDataset is covered by sort_permute_iter's tests
SortedProbeDataset::ProbeContext makeProbeContext( int hitCounter, float distance = 10 ) {
	SortedProbeDataset::ProbeContext probeContext;
	probeContext.hitCounter = hitCounter;
	probeContext.distance = distance;
	probeContext.Lab.x = probeContext.Lab.y = probeContext.Lab.z = 0;
	return probeContext;
}

TEST( IndexedProbeDataset, idAndWeight ) {
	RawProbeDataset rawDataset;

	const int minHitCounter = 3;
	const int maxHitCounter = OptixProgramInterface::numProbeSamples - 3;

	const int bucketSize = 5;

	for( int i = minHitCounter ; i <= maxHitCounter ; i++ ) {
		for( int j = 0 ; j < bucketSize ; j++ ) {
			rawDataset.push_back( makeProbeContext( i, j ) );	
		}
	}
	IndexedProbeDataset dataset = SortedProbeDataset( rawDataset );

	ASSERT_EQ( rawDataset.size(), dataset.size() );
	for( int i = 0 ; i < dataset.size() ; i++ ) {
		ASSERT_EQ( i, dataset.getProbeContexts()[ i ].probeIndex );
		ASSERT_EQ( 1, dataset.getProbeContexts()[ i ].weight );
	}
}

TEST( IndexedProbeDataset, setHitCounterLowerBounds ) {
	RawProbeDataset rawDataset;

	const int minHitCounter = 3;
	const int maxHitCounter = OptixProgramInterface::numProbeSamples - 3;

	const int bucketSize = 5;

	for( int i = minHitCounter ; i <= maxHitCounter ; i++ ) {
		for( int j = bucketSize - 1 ; j >= 0 ; j-- ) {
			rawDataset.push_back( makeProbeContext( i, j ) );
		}
	}
	IndexedProbeDataset dataset = SortedProbeDataset( rawDataset );

	ASSERT_EQ( rawDataset.size(), dataset.size() );

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

TEST( SortedProbeDataset, subSet ) {
	RawProbeDataset rawDataset;

	for( int i = 0 ; i < 1000 ; i++ ) {
		rawDataset.push_back( makeProbeContext( 1, 2 * i ) );
	}
	for( int i = 0 ; i < 1000 ; i++ ) {
		rawDataset.push_back( makeProbeContext( 0, 2*i + 1 ) );
	}

	SortedProbeDataset dataset = SortedProbeDataset( rawDataset );

	SortedProbeDataset scratch = dataset.subSet( std::make_pair( 0, 2000 ) );

	for( int j = 0 ; j < 2000 ; j++ ) {
		ASSERT_EQ( j, scratch.getProbeContexts()[j].distance );
	}
}

TEST( SortedProbeDataset, merge ) {
	RawProbeDataset first, second;

	for( int i = 0 ; i < 1000 ; i++ ) {
		first.push_back( makeProbeContext( 0, 2*i ) );
		second.push_back( makeProbeContext( 0, 2*i + 1 ) );
	}

	SortedProbeDataset result = SortedProbeDataset::merge( SortedProbeDataset( first ), SortedProbeDataset( second ) );

	for( int j = 0 ; j < 2000 ; j++ ) {
		ASSERT_EQ( j, result.getProbeContexts()[j].distance );
	}
}

TEST( IndexedProbeDataset, mergeMultiple ) {
	const int numDatasets = 10;
	SortedProbeDataset datasets[numDatasets];

	for( int j = 0 ; j < numDatasets ; j++ ) {
		RawProbeDataset rawDataset;
		for( int i = 0 ; i < 1000 ; i++ ) {
			rawDataset.push_back( makeProbeContext( 0, numDatasets * i + j ) );
		}
		datasets[j] = SortedProbeDataset( rawDataset );
	}

	std::vector< const SortedProbeDataset * > pDatasets;
	for( int j = 0 ; j < numDatasets ; j++ ) {
		pDatasets.push_back( &datasets[j] );
	}

	SortedProbeDataset result = SortedProbeDataset::mergeMultiple( pDatasets );

	for( int j = 0 ; j < numDatasets * 1000 ; j++ ) {
		ASSERT_EQ( j, result.getProbeContexts()[j].distance );
	}
}

TEST( IndexedProbeDataset, mergeMultiple_empty ) {
	const int numDatasets = 10;
	SortedProbeDataset datasets[numDatasets];

	std::vector< const SortedProbeDataset * > pDatasets;
	for( int j = 0 ; j < numDatasets ; j++ ) {
		pDatasets.push_back( &datasets[j] );
	}

	SortedProbeDataset result = SortedProbeDataset::mergeMultiple( pDatasets );

	ASSERT_EQ( result.size(), 0 );
}

TEST( ProbeDatabase, zeroTolerance ) {
	// init the dataset
	RawProbeDataset rawDataset, rawTestDataset;
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawDataset.push_back( makeProbeContext( j, i ) );
			rawTestDataset.push_back( makeProbeContext( j, 500 + i ) );
		}
	}

	SortedProbeDataset dataset = SortedProbeDataset( rawDataset ), testDataset = SortedProbeDataset( rawTestDataset );

	auto probes = std::vector< ProbeDatabase::Probe >( 5*1000 );

	ProbeDatabase candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, probes, dataset.clone() );
	candidateFinder.integrateDatasets();

	{
		ProbeDatabase::Query query( candidateFinder );

		query.setQueryDataset( dataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::MatchInfos matchInfos = query.getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 5000 );
		EXPECT_FLOAT_EQ( matchInfos[0].probeMatchPercentage, 1.0);
		EXPECT_FLOAT_EQ( matchInfos[0].queryMatchPercentage, 1.0);
		EXPECT_EQ( matchInfos[0].id, 0 );
	}

	{
		ProbeDatabase::Query query( candidateFinder );

		query.setQueryDataset( testDataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::MatchInfos matchInfos = query.getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 2500 );
		EXPECT_FLOAT_EQ( matchInfos[0].probeMatchPercentage, 0.5);
		EXPECT_FLOAT_EQ( matchInfos[0].queryMatchPercentage, 0.5);
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
}

TEST( ProbeDatabase, zeroTolerance_biggerDB ) {
	// init the dataset
	RawProbeDataset rawDataset, rawTestDataset;
	for( int i = 0 ; i < 1500 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawDataset.push_back( makeProbeContext( j, i ) );
		}
	}
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawTestDataset.push_back( makeProbeContext( j, 1000 + i ) );
		}
	}

	SortedProbeDataset dataset = SortedProbeDataset( rawDataset ), testDataset = SortedProbeDataset( rawTestDataset );

	auto probes = std::vector< ProbeDatabase::Probe >( 5*1000 );

	ProbeDatabase candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, probes, dataset.clone() );
	candidateFinder.integrateDatasets();

	{
		ProbeDatabase::Query query( candidateFinder );

		query.setQueryDataset( dataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::MatchInfos matchInfos = query.getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 5*1500 );
		EXPECT_FLOAT_EQ( matchInfos[0].probeMatchPercentage, 1.0);
		EXPECT_FLOAT_EQ( matchInfos[0].queryMatchPercentage, 1.0);
		EXPECT_EQ( matchInfos[0].id, 0 );
	}

	{
		ProbeDatabase::Query query( candidateFinder );

		query.setQueryDataset( testDataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::MatchInfos matchInfos = query.getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 5*500 );
		EXPECT_FLOAT_EQ( matchInfos[0].probeMatchPercentage, 500.0/1500.0);
		EXPECT_FLOAT_EQ( matchInfos[0].queryMatchPercentage, 500.0/1000.0);
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
}

TEST( ProbeDatabase, oneTolerance ) {
	// init the dataset
	RawProbeDataset rawDataset, rawTestDataset;
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawDataset.push_back( makeProbeContext( j, i ) );
			rawTestDataset.push_back( makeProbeContext( j, 500 + i ) );
		}
	}

	SortedProbeDataset dataset = SortedProbeDataset( rawDataset ), testDataset = SortedProbeDataset( rawTestDataset );

	auto probes = std::vector< ProbeDatabase::Probe >( 5*1000 );

	ProbeDatabase candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, probes, dataset.clone() );
	candidateFinder.integrateDatasets();

	ProbeContextTolerance pct;
	pct.occusionTolerance = 1.0 / (OptixProgramInterface::numProbeSamples + 1);
	pct.distanceTolerance = 0;

	{
		ProbeDatabase::Query query( candidateFinder );

		query.setQueryDataset( dataset.clone() );
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::MatchInfos matchInfos = query.getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 2*2*1000 + 3*3*1000 );
		EXPECT_FLOAT_EQ( matchInfos[0].probeMatchPercentage, 1.0);
		EXPECT_FLOAT_EQ( matchInfos[0].queryMatchPercentage, 1.0);
		EXPECT_EQ( matchInfos[0].id, 0 );
	}

	{
		ProbeDatabase::Query query( candidateFinder );

		query.setQueryDataset( testDataset.clone() );
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::MatchInfos matchInfos = query.getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 2*2*500 + 3*3*500 );
		EXPECT_FLOAT_EQ( matchInfos[0].probeMatchPercentage, 0.5);
		EXPECT_FLOAT_EQ( matchInfos[0].queryMatchPercentage, 0.5);
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
}

#if 1
TEST( ProbeDatabase, big ) {
	// init the dataset
	RawProbeDataset rawDataset, rawTestDataset;
	for( int i = 0 ; i < 20000 ; i++ ) {
		for( int j = 0 ; j < 30 ; j++ ) {
			rawDataset.push_back( makeProbeContext( j, i ) );
			rawTestDataset.push_back( makeProbeContext( j, 10000 + i ) );
		}
	}

	auto probes = std::vector< ProbeDatabase::Probe >( rawDataset.size() );

	SortedProbeDataset dataset = SortedProbeDataset( rawDataset ), testDataset = SortedProbeDataset( rawTestDataset );

	ProbeDatabase candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, probes, dataset.clone() );
	candidateFinder.integrateDatasets();

	{
		ProbeDatabase::Query query( candidateFinder );

		query.setQueryDataset( dataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::MatchInfos matchInfos = query.getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 600000 );
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
	{
		ProbeDatabase::Query query( candidateFinder );

		query.setQueryDataset( testDataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query.setProbeContextTolerance( pct );

		query.execute();

		ProbeDatabase::Query::MatchInfos matchInfos = query.getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 300000 );
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
}
#endif
