#include "candidateFinderInterface.h"
#include "gtest.h"

TEST( probeContext_lexicographicalLess, order ) {
	ProbeContext a, b;
	{
		a.hitCounter = 10;
		b.hitCounter = 20;

		ASSERT_TRUE( probeContext_lexicographicalLess( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = 10;
		b.distance = 20;

		ASSERT_TRUE( probeContext_lexicographicalLess( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.color.x = 10;
		b.color.x = 20;

		ASSERT_TRUE( probeContext_lexicographicalLess( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.color.x = a.color.x = 10;

		a.color.y = 10;
		b.color.y = 20;

		ASSERT_TRUE( probeContext_lexicographicalLess( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.color.x = a.color.x = 10;
		a.color.y = a.color.y = 10;

		a.color.z = 10;
		b.color.z = 20;

		ASSERT_TRUE( probeContext_lexicographicalLess( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess( b, a ) );
	}
}

// ProbeDataset is covered by sort_permute_iter's tests

ProbeContext makeProbeContext( int hitCounter, float distance = 10 ) {
	ProbeContext probeContext;
	probeContext.hitCounter = hitCounter;
	probeContext.distance = distance;
	probeContext.color.x = probeContext.color.y = probeContext.color.z = 0;
	return probeContext;
}

TEST( ProbeDataset, setHitCounterLowerBounds ) {
	ProbeDataset dataset;

	const int minHitCounter = 3;
	const int maxHitCounter = OptixProgramInterface::numProbeSamples - 3; 

	const int bucketSize = 5;

	for( int i = minHitCounter ; i <= maxHitCounter ; i++ ) {
		for( int j = bucketSize - 1 ; j >= 0 ; j-- ) {
			dataset.probeContexts.push_back( makeProbeContext( i, j ) );
		}
	}
	dataset.probes.resize( dataset.probeContexts.size() );

	dataset.sort();
	dataset.setHitCounterLowerBounds();

	ASSERT_EQ( dataset.hitCounterLowerBounds.size(), OptixProgramInterface::numProbeSamples + 2 );
	for( int i = 0 ; i < minHitCounter ; i++ ) {
		EXPECT_EQ( dataset.hitCounterLowerBounds[i], 0 );
	}
	int lowerBound = 0;
	for( int i = minHitCounter ; i <= maxHitCounter; i++, lowerBound += bucketSize ) {
		EXPECT_EQ( dataset.hitCounterLowerBounds[i], lowerBound );
	}
	for( int i = maxHitCounter + 1 ; i <= OptixProgramInterface::numProbeSamples + 1 ; ++i ) {
		EXPECT_EQ( dataset.hitCounterLowerBounds[i], dataset.probeContexts.size() );
	}
}

TEST( ProbeDataset, merge ) {
	ProbeDataset first, second;

	for( int i = 0 ; i < 1000 ; i++ ) {
		first.probeContexts.push_back( makeProbeContext( 0, 2*i ) );
		second.probeContexts.push_back( makeProbeContext( 0, 2*i + 1 ) );
	}
	first.probes.resize( 1000 );
	second.probes.resize( 1000 );

	ProbeDataset result = ProbeDataset::merge( first, second );

	for( int j = 0 ; j < 2000 ; j++ ) {
		ASSERT_EQ( j, result.probeContexts[j].distance );
	}
}

TEST( ProbeDataset, mergeMultiple ) {
	const int numDatasets = 10;
	ProbeDataset datasets[numDatasets];

	for( int j = 0 ; j < numDatasets ; j++ ) {
		for( int i = 0 ; i < 1000 ; i++ ) {
			datasets[j].probeContexts.push_back( makeProbeContext( 0, numDatasets * i + j ) );	
		}
		datasets[j].probes.resize( 1000 );
	}
	

	std::vector< ProbeDataset * > pDatasets;
	for( int j = 0 ; j < numDatasets ; j++ ) {
		pDatasets.push_back( &datasets[j] );
	}

	ProbeDataset result = ProbeDataset::mergeMultiple( pDatasets );

	for( int j = 0 ; j < numDatasets * 1000 ; j++ ) {
		ASSERT_EQ( j, result.probeContexts[j].distance );
	}
}

TEST( ProbeDataset, mergeMultiple_empty ) {
	const int numDatasets = 10;
	ProbeDataset datasets[numDatasets];

	std::vector< ProbeDataset * > pDatasets;
	for( int j = 0 ; j < numDatasets ; j++ ) {
		pDatasets.push_back( &datasets[j] );
	}

	ProbeDataset result = ProbeDataset::mergeMultiple( pDatasets );

	ASSERT_EQ( result.size(), 0 );
}

TEST( CandidateFinder, zeroTolerance ) {
	ProbeDataset dataset, testDataset;

	// init the dataset
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			dataset.probeContexts.push_back( makeProbeContext( j, i ) );
			testDataset.probeContexts.push_back( makeProbeContext( j, 500 + i ) );
		}
	}
	dataset.probes.resize( dataset.probeContexts.size() );
	testDataset.probes.resize( dataset.probeContexts.size() );

	CandidateFinder candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, dataset.clone() );
	candidateFinder.integrateDatasets();

	{
		auto query = candidateFinder.createQuery();

		query->setQueryDataset( dataset.clone() );
		
		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query->setProbeContextTolerance( pct );

		query->execute();

		CandidateFinder::Query::MatchInfos matchInfos = query->getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 5000 );
		EXPECT_EQ( matchInfos[0].id, 0 );
	}

	{
		auto query = candidateFinder.createQuery();

		query->setQueryDataset( testDataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query->setProbeContextTolerance( pct );

		query->execute();

		CandidateFinder::Query::MatchInfos matchInfos = query->getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 2500 );
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
}

TEST( CandidateFinder, oneTolerance ) {
	ProbeDataset dataset, testDataset;

	// init the dataset
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			dataset.probeContexts.push_back( makeProbeContext( j, i ) );
			testDataset.probeContexts.push_back( makeProbeContext( j, 500 + i ) );
		}
	}
	dataset.probes.resize( dataset.probeContexts.size() );
	testDataset.probes.resize( dataset.probeContexts.size() );

	CandidateFinder candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, dataset.clone() );
	candidateFinder.integrateDatasets();

	ProbeContextTolerance pct;
	pct.occusionTolerance = 1.0 / (OptixProgramInterface::numProbeSamples + 1);
	pct.distanceTolerance = 0;

	{
		auto query = candidateFinder.createQuery();

		query->setQueryDataset( dataset.clone() );
		query->setProbeContextTolerance( pct );

		query->execute();

		CandidateFinder::Query::MatchInfos matchInfos = query->getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 2*2000 + 3*3000 );
		EXPECT_EQ( matchInfos[0].id, 0 );
	}

	{
		auto query = candidateFinder.createQuery();

		query->setQueryDataset( testDataset.clone() );
		query->setProbeContextTolerance( pct );

		query->execute();

		CandidateFinder::Query::MatchInfos matchInfos = query->getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, (2*2000 + 3*3000) / 2 );
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
}

#if 1
TEST( CandidateFinder, big ) {
	ProbeDataset dataset, testDataset;

	// init the dataset
	for( int i = 0 ; i < 20000 ; i++ ) {
		for( int j = 0 ; j < 30 ; j++ ) {
			dataset.probeContexts.push_back( makeProbeContext( j, i ) );
			testDataset.probeContexts.push_back( makeProbeContext( j, 10000 + i ) );
		}
	}
	dataset.probes.resize( dataset.probeContexts.size() );
	testDataset.probes.resize( dataset.probeContexts.size() );

	CandidateFinder candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, dataset.clone() );
	candidateFinder.integrateDatasets();

	{
		auto query = candidateFinder.createQuery();

		query->setQueryDataset( dataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query->setProbeContextTolerance( pct );

		query->execute();

		CandidateFinder::Query::MatchInfos matchInfos = query->getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 600000 );
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
	{
		auto query = candidateFinder.createQuery();

		query->setQueryDataset( testDataset.clone() );

		ProbeContextTolerance pct;
		pct.occusionTolerance = 0;
		pct.distanceTolerance = 0;
		query->setProbeContextTolerance( pct );

		query->execute();

		CandidateFinder::Query::MatchInfos matchInfos = query->getCandidates();

		ASSERT_EQ( matchInfos.size(), 1 );
		EXPECT_EQ( matchInfos[0].numMatches, 300000 );
		EXPECT_EQ( matchInfos[0].id, 0 );
	}
}
#endif