#include "neighborhoodDatabase.h"

#include "gtest.h"

using namespace Neighborhood;

//////////////////////////////////////////////////////////////////////////
// V2
//

using namespace Neighborhood;

TEST( NeighborhoodDatabaseV2_SortedDataset, all ) {
	RawIdDistances rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}
	}

	NeighborhoodContext sortedDataset( std::move( rawDataset ) );

	ASSERT_EQ( 100, sortedDataset.getDistancesById().size() );
	for( int id = 0 ; id < 100 ; id++ ) {
		const auto &distances = sortedDataset.getDistancesById()[id];

		for( int distance = 0 ; distance < 10 ; distance++ ) {
			EXPECT_FLOAT_EQ( float( distance ), distances[ distance ] );
		}
	}
}

TEST( NeighborhoodDatabaseV2, getEntryById ) {
	NeighborhoodDatabaseV2 db;

	auto &a = db.getSampledModel( 10 );
	auto &b = db.getSampledModel( 10 );

	ASSERT_EQ( &a, &b );

	auto &c = db.getSampledModel( 100 );

	ASSERT_NE( &b, &c );
}

TEST( NeighborhoodDatabaseV2_Entry, addInstance ) {
	NeighborhoodDatabaseV2 db;

	const auto &constEntry = db.getSampledModel( 1 );

	RawIdDistances rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}
	}

	ASSERT_TRUE( constEntry.instances.empty() );

	db.addInstance( 1, std::move( rawDataset ) );

	ASSERT_EQ( 1, constEntry.instances.size() );
}

TEST( NeighborhoodDatabaseV2_Query, all ) {
	NeighborhoodDatabaseV2 db;

	ModelDatabase modelDatabase( nullptr );

	RawIdDistances rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}

		ModelDatabase::ModelInformation info;
		info.area = info.diagonalLength = info.volume = 1.0;
		modelDatabase.informationById.emplace_back( std::move( info ) );
	}

	db.modelDatabase = &modelDatabase;

	db.addInstance( 1, RawIdDistances( rawDataset ) );

	NeighborhoodDatabaseV2::Query query( db, 2.0, std::move( rawDataset ) );

	const auto results = query.execute();

	ASSERT_EQ( 1, results.size() );
	EXPECT_FLOAT_EQ( 1.0f, results.front().first );
	EXPECT_EQ( 1, results.front().second );
}

static void mockModelDatabaseWith( ModelDatabase &modelDatabase, int numModels ) {
	for( int i = 0 ; i < numModels ; i++ ) {
		ModelDatabase::ModelInformation info;
		info.area = info.diagonalLength = info.volume = 1.0;
		modelDatabase.informationById.emplace_back( std::move( info ) );
	}
}

template< int numDistances, int numModels >
static RawIdDistances createQueryDataset( const int queryNumInstances[ numDistances ][ numModels ] ) {
	RawIdDistances rawDataset;

	for( int distanceIndex = 0 ; distanceIndex < numDistances ; distanceIndex++ ) {
		for( int modelIndex = 0 ; modelIndex < numModels ; modelIndex++ ) {
			const int numInstances = queryNumInstances[ distanceIndex ][ modelIndex ];

			for( int instanceIndex = 0 ; instanceIndex < numInstances ; instanceIndex++ ) {
				rawDataset.push_back( std::make_pair<int, float>( modelIndex, distanceIndex + 1 ) );
			}
		}
	}

	return rawDataset;
}

template< int numModels >
static void addSimpleScene( int distance, NeighborhoodDatabaseV2 &database, const int sceneNumInstances[numModels] ) {
	for( int modelIndex = 0 ; modelIndex < numModels ; modelIndex++ ) {
		const int numInstances = sceneNumInstances[ modelIndex ];

		for( int instanceIndex = 0 ; instanceIndex < numInstances ; instanceIndex++ ) {
			RawIdDistances rawDataset;

			for( int otherModelIndex = 0 ; otherModelIndex < numModels ; otherModelIndex++ ) {
				const int numOtherInstances = sceneNumInstances[ otherModelIndex ];

				for( int otherInstanceIndex = 0 ; otherInstanceIndex < numOtherInstances ; otherInstanceIndex++ ) {
					if(
							otherInstanceIndex != instanceIndex
						||
							otherModelIndex != modelIndex
					) {
						rawDataset.push_back( std::make_pair<int, float>( otherModelIndex, distance ) );
					}
				}
			}

			database.addInstance( modelIndex, std::move( rawDataset ) );
		}
	}
}

/* Case 1:
 * all distances are equal, so we only test the probability
 *
 * A, B
 *
 * B, C
 *
 * Test queries
 *
 * A Result: B
 *
 * B Result: A and C with equal prob
 *
 * C Result: B
 */
TEST( NeighborhoodDatabase_Query, testcase1 ) {
	ModelDatabase modelDatabase( nullptr );
	mockModelDatabaseWith( modelDatabase, 3 );

	NeighborhoodDatabaseV2 db;
	db.modelDatabase = &modelDatabase;

	int datasets[2][3] = {
		{ 1, 1, 0 },
		{ 0, 1, 1 }
	};


	for( int i = 0 ; i < 2 ; i++ ) {
		addSimpleScene< 3 >( 1, db, datasets[i] );
	}

	int queries[3][1][3] = {
		{{ 1, 0, 0 }},
		{{ 0, 1, 0 }},
		{{ 0, 0, 1 }}
	};

	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 3 >( queries[0] ) );

		const auto results = query.execute();

		ASSERT_EQ( 3, results.size() );
		EXPECT_EQ( 1, results[0].second );
		EXPECT_FLOAT_EQ( 1.0f, results[0].first);
		EXPECT_LT( results[1].first, results[0].first );
	}
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 3 >( queries[1] ) );

		const auto results = query.execute();

		ASSERT_EQ( 3, results.size() );
		EXPECT_FLOAT_EQ( 1.0f, results[0].first);
		EXPECT_FLOAT_EQ( 1.0f, results[1].first);
		EXPECT_LT( results[2].first, 1.0 );
	}
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 3 >( queries[2] ) );

		const auto results = query.execute();

		ASSERT_EQ( 3, results.size() );
		EXPECT_EQ( 1, results[0].second );
		EXPECT_FLOAT_EQ( 1.0f, results[0].first);
		EXPECT_LT( results[1].first, results[0].first );
	}
}


/* Case 2:
 * all distances are equal, so we only test the probability
 *
 * A, B, B, B
 *
 * A, B, B, B, B, B, B
 *
 * Test queries
 *
 * B, B, B
 * Result: A
 *
 * B, B, B, B, B, B
 * Result: A
 *
 * C Result: B
 */
TEST( NeighborhoodDatabase_Query, testcase2 ) {
	ModelDatabase modelDatabase( nullptr );
	mockModelDatabaseWith( modelDatabase, 3 );

	NeighborhoodDatabaseV2 db;
	db.modelDatabase = &modelDatabase;

	int datasets[][2] = {
		{ 1, 3 },
		{ 1, 6 }
	};

	for( int i = 0 ; i < 2 ; i++ ) {
		addSimpleScene< 2 >( 1, db, datasets[i] );
	}

	int queries[][1][2] = {
		{{ 0, 3 }},
		{{ 0, 6 }},
		{{ 0, 4 }},
		{{ 0, 5 }}
	};

	float perfectMatchProb;
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 2 >( queries[0] ) );

		const auto results = query.execute();

		ASSERT_EQ( 2, results.size() );
		EXPECT_EQ( 0, results[0].second );

		perfectMatchProb = results[0].first;
	}
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 2 >( queries[1] ) );

		const auto results = query.execute();

		ASSERT_EQ( 2, results.size() );
		EXPECT_EQ( 0, results[0].second );

		perfectMatchProb = std::min( perfectMatchProb, results[0].first );
	}
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 2 >( queries[2] ) );

		const auto results = query.execute();

		ASSERT_EQ( 2, results.size() );
		EXPECT_LT( results[0].first, perfectMatchProb );
	}
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 2 >( queries[3] ) );

		const auto results = query.execute();

		ASSERT_EQ( 2, results.size() );
		EXPECT_LT( results[0].first, perfectMatchProb );
	}
}

/* Case 3:
 * all distances are equal, so we only test the probability
 *
 * A; - - -;  B, B, B
 *
 * C (B, B, B), (B, B, B)
 *
 * Test queries
 *
 * (B, B, B), (B, B, B) 
 * Result: A, C
 *
 * (B, B, B), (B, B, B) 
 * Result: A, C
 *
 */
TEST( NeighborhoodDatabase_Query, testcase3 ) {
	/* TODO */
	ModelDatabase modelDatabase( nullptr );
	mockModelDatabaseWith( modelDatabase, 3 );

	NeighborhoodDatabaseV2 db;
	db.modelDatabase = &modelDatabase;

	int datasets[][2] = {
		{ 1, 3 },
		{ 1, 6 }
	};

	for( int i = 0 ; i < 2 ; i++ ) {
		addSimpleScene< 2 >( 1, db, datasets[i] );
	}

	int queries[][1][2] = {
		{{ 0, 3 }},
		{{ 0, 6 }},
		{{ 0, 4 }},
		{{ 0, 5 }}
	};

	float perfectMatchProb;
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 2 >( queries[0] ) );

		const auto results = query.execute();

		ASSERT_EQ( 2, results.size() );
		EXPECT_EQ( 0, results[0].second );

		perfectMatchProb = results[0].first;
	}
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 2 >( queries[1] ) );

		const auto results = query.execute();

		ASSERT_EQ( 2, results.size() );
		EXPECT_EQ( 0, results[0].second );

		perfectMatchProb = std::min( perfectMatchProb, results[0].first );
	}
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 2 >( queries[2] ) );

		const auto results = query.execute();

		ASSERT_EQ( 2, results.size() );
		EXPECT_LT( results[0].first, perfectMatchProb );
	}
	{
		NeighborhoodDatabaseV2::Query query( db, 1.0, createQueryDataset< 1, 2 >( queries[3] ) );

		const auto results = query.execute();

		ASSERT_EQ( 2, results.size() );
		EXPECT_LT( results[0].first, perfectMatchProb );
	}
}


/* Fail Case 1:
 * all distances are equal, so we only test the probability
 *
 * A, (B, B, B), (B, B, B) 
 *
 * C, (B, B, B), (B, B, B)
 *
 * Test queries
 *
 * (B, B, B), (B, B, B) 
 * Result: A, C
 *
 * (B, B, B), (B, B, B) 
 * Result: A, C
 *
 */