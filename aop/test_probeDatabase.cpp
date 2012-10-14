#include "probeDatabase.h"
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

		ASSERT_TRUE( probeContext_lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.color.x = 10;
		b.color.x = 20;

		ASSERT_TRUE( probeContext_lexicographicalLess( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess( b, a ) );

		ASSERT_TRUE( probeContext_lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess_startWithDistance( b, a ) );
	}
	{
		a.hitCounter = b.hitCounter = 10;

		a.distance = b.distance = 10;
		a.color.x = a.color.x = 10;

		a.color.y = 10;
		b.color.y = 20;

		ASSERT_TRUE( probeContext_lexicographicalLess( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess( b, a ) );

		ASSERT_TRUE( probeContext_lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess_startWithDistance( b, a ) );
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

		ASSERT_TRUE( probeContext_lexicographicalLess_startWithDistance( a, b ) );
		ASSERT_FALSE( probeContext_lexicographicalLess_startWithDistance( b, a ) );
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

TEST( IndexedProbeDataset, setHitCounterLowerBounds ) {
	RawProbeDataset rawDataset;

	const int minHitCounter = 3;
	const int maxHitCounter = OptixProgramInterface::numProbeSamples - 3;

	const int bucketSize = 5;

	for( int i = minHitCounter ; i <= maxHitCounter ; i++ ) {
		for( int j = bucketSize - 1 ; j >= 0 ; j-- ) {
			rawDataset.probeContexts.push_back( makeProbeContext( i, j ) );
		}
	}
	rawDataset.probes.resize( rawDataset.probeContexts.size() );

	IndexedProbeDataset dataset( std::move( rawDataset ) );

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
		rawDataset.probeContexts.push_back( makeProbeContext( 1, 2 * i ) );
	}
	for( int i = 0 ; i < 1000 ; i++ ) {
		rawDataset.probeContexts.push_back( makeProbeContext( 0, 2*i + 1 ) );
	}
	rawDataset.probes.resize( 2000 );

	SortedProbeDataset dataset( std::move( rawDataset ) );

	dataset.subSet( std::make_pair( 0, 2000 ) );

	SortedProbeDataset scratch = dataset.subSet( std::make_pair( 0, 2000 ) );

	for( int j = 0 ; j < 2000 ; j++ ) {
		ASSERT_EQ( j, scratch.getProbeContexts()[j].distance );
	}
}

TEST( SortedProbeDataset, merge ) {
	RawProbeDataset first, second;

	for( int i = 0 ; i < 1000 ; i++ ) {
		first.probeContexts.push_back( makeProbeContext( 0, 2*i ) );
		second.probeContexts.push_back( makeProbeContext( 0, 2*i + 1 ) );
	}
	first.probes.resize( 1000 );
	second.probes.resize( 1000 );

	SortedProbeDataset result = SortedProbeDataset::merge( std::move( first ), std::move( second ) );

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
			rawDataset.probeContexts.push_back( makeProbeContext( 0, numDatasets * i + j ) );
		}
		rawDataset.probes.resize( 1000 );
		datasets[j] = std::move( rawDataset );
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
			rawDataset.probeContexts.push_back( makeProbeContext( j, i ) );
			rawTestDataset.probeContexts.push_back( makeProbeContext( j, 500 + i ) );
		}
	}
	rawDataset.probes.resize( rawDataset.probeContexts.size() );
	rawTestDataset.probes.resize( rawTestDataset.probeContexts.size() );

	SortedProbeDataset dataset( std::move( rawDataset ) ), testDataset( std::move( rawTestDataset ) );

	ProbeDatabase candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, dataset.clone() );
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
			rawDataset.probeContexts.push_back( makeProbeContext( j, i ) );
		}
	}
	for( int i = 0 ; i < 1000 ; i++ ) {
		for( int j = 0 ; j < 5 ; j++ ) {
			rawTestDataset.probeContexts.push_back( makeProbeContext( j, 1000 + i ) );
		}
	}
	rawDataset.probes.resize( rawDataset.probeContexts.size() );
	rawTestDataset.probes.resize( rawTestDataset.probeContexts.size() );

	SortedProbeDataset dataset( std::move( rawDataset ) ), testDataset( std::move( rawTestDataset ) );

	ProbeDatabase candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, dataset.clone() );
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
			rawDataset.probeContexts.push_back( makeProbeContext( j, i ) );
			rawTestDataset.probeContexts.push_back( makeProbeContext( j, 500 + i ) );
		}
	}
	rawDataset.probes.resize( rawDataset.probeContexts.size() );
	rawTestDataset.probes.resize( rawTestDataset.probeContexts.size() );

	SortedProbeDataset dataset( std::move( rawDataset ) ), testDataset( std::move( rawTestDataset ) );

	ProbeDatabase candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, dataset.clone() );
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

#if 0
TEST( ProbeDatabase, big ) {
	// init the dataset
	RawProbeDataset rawDataset, rawTestDataset;
	for( int i = 0 ; i < 20000 ; i++ ) {
		for( int j = 0 ; j < 30 ; j++ ) {
			rawDataset.probeContexts.push_back( makeProbeContext( j, i ) );
			rawTestDataset.probeContexts.push_back( makeProbeContext( j, 10000 + i ) );
		}
	}
	rawDataset.probes.resize( rawDataset.probeContexts.size() );
	rawTestDataset.probes.resize( rawTestDataset.probeContexts.size() );

	SortedProbeDataset dataset( std::move( rawDataset ) ), testDataset( std::move( rawTestDataset ) );

	ProbeDatabase candidateFinder;
	candidateFinder.reserveIds( 0 );
	candidateFinder.addDataset( 0, dataset.clone() );
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