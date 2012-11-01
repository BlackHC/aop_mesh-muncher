#include "neighborhoodDatabase.h"

#include "gtest.h"

using namespace Neighborhood;

//////////////////////////////////////////////////////////////////////////
// V2
//

TEST( NeighborhoodDatabaseV2_SortedDataset, all ) {
	NeighborhoodDatabaseV2::RawIdDistances rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}
	}

	NeighborhoodDatabaseV2::NeighborhoodContext sortedDataset( std::move( rawDataset ) );

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

	NeighborhoodDatabaseV2::RawIdDistances rawDataset;

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

	NeighborhoodDatabaseV2::RawIdDistances rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}

		ModelDatabase::ModelInformation info;
		info.area = info.diagonalLength = info.volume = 1.0;
		modelDatabase.informationById.emplace_back( std::move( info ) );
	}

	db.modelDatabase = &modelDatabase;

	db.addInstance( 1, std::move( NeighborhoodDatabaseV2::RawIdDistances( rawDataset ) ) );

	NeighborhoodDatabaseV2::Query query( db, 2.0, std::move( rawDataset ) );

	const auto results = query.execute();

	ASSERT_EQ( 1, results.size() );
	EXPECT_FLOAT_EQ( 1.0f, results.front().first );
	EXPECT_EQ( 1, results.front().second );
}