#include "neighborhoodDatabase.h"

#include "gtest.h"

TEST( NeighborhoodDatabase_SortedDataset, all ) {
	NeighborhoodDatabase::RawDataset rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}
	}

	NeighborhoodDatabase::SortedDataset sortedDataset( std::move( rawDataset ) );
	
	ASSERT_EQ( 100, sortedDataset.getDistancesById().size() );
	for( int id = 0 ; id < 100 ; id++ ) {
		const auto &idDistancesPair = sortedDataset.getDistancesById()[id];
		
		EXPECT_EQ( id, idDistancesPair.first );

		for( int distance = 0 ; distance < 10 ; distance++ ) {
			EXPECT_FLOAT_EQ( float( distance ), idDistancesPair.second[ distance ] );	
		}
	}
}

TEST( NeighborhoodDatabase_Dataset, getNumBins ) {
	EXPECT_EQ( 3, NeighborhoodDatabase::Dataset::getNumBins( 1.0, 1.0 ) );
	EXPECT_EQ( 3, NeighborhoodDatabase::Dataset::getNumBins( 1.0, 0.1 ) );
	EXPECT_EQ( 3, NeighborhoodDatabase::Dataset::getNumBins( 1.0, 0.9 ) );

	EXPECT_EQ( 3, NeighborhoodDatabase::Dataset::getNumBins( 2.0, 2.0 ) );
	EXPECT_EQ( 3, NeighborhoodDatabase::Dataset::getNumBins( 2.0, 0.1 ) );
	EXPECT_EQ( 3, NeighborhoodDatabase::Dataset::getNumBins( 2.0, 1.8 ) );

	EXPECT_EQ( 5, NeighborhoodDatabase::Dataset::getNumBins( 1.0, 2.0 ) );
	EXPECT_EQ( 5, NeighborhoodDatabase::Dataset::getNumBins( 1.0, 1.8 ) );

	EXPECT_EQ( 9, NeighborhoodDatabase::Dataset::getNumBins( 0.5, 2.0 ) );
	EXPECT_EQ( 9, NeighborhoodDatabase::Dataset::getNumBins( 0.5, 1.8 ) );
}

TEST( NeighborhoodDatabase_Dataset, construction ) {
	NeighborhoodDatabase::RawDataset rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}
	}

	NeighborhoodDatabase::SortedDataset sortedDataset( std::move( rawDataset ) );

	NeighborhoodDatabase::Dataset dataset( 2.0, 10.0, sortedDataset );

	ASSERT_EQ( 100, dataset.getBinsById().size() );

	for( int id = 0 ; id < 100 ; id++ ) {
		const auto &idBinsPair = dataset.getBinsById()[id];

		ASSERT_EQ( id, idBinsPair.first );
		ASSERT_EQ( 11, idBinsPair.second.size() );

		EXPECT_EQ( 1, idBinsPair.second[0] );
		EXPECT_EQ( 1, idBinsPair.second[10] );
		for( int bin = 1 ; bin < 11 - 1 ; bin++ ) {
			EXPECT_EQ( 2, idBinsPair.second[bin] );
		}
	}
}

TEST( NeighborhoodDatabase_Dataset, construction_maxDistance ) {
	NeighborhoodDatabase::RawDataset rawDataset;

	rawDataset.push_back( std::make_pair( 0, 10.0 ) );

	NeighborhoodDatabase::SortedDataset sortedDataset( std::move( rawDataset ) );

	NeighborhoodDatabase::Dataset dataset( 2.0, 10.0, sortedDataset );

	ASSERT_EQ( 1, dataset.getBinsById().size() );

	const auto &idBinsPair = dataset.getBinsById()[0];

	ASSERT_EQ( 0, idBinsPair.first );
	ASSERT_EQ( 11, idBinsPair.second.size() );

	for( int bin = 0 ; bin < 11 ; bin++ ) {
		EXPECT_EQ( 0, idBinsPair.second[bin] );
	}
}

TEST( NeighborhoodDatabase, getEntryById ) {
	NeighborhoodDatabase db;

	auto &a = db.getEntryById( 10 );
	auto &b = db.getEntryById( 10 );

	ASSERT_EQ( &a, &b );

	auto &c = db.getEntryById( 100 );

	ASSERT_NE( &b, &c );
}

TEST( NeighborhoodDatabase_Entry, addInstance ) {
	NeighborhoodDatabase db;

	auto &entry = db.getEntryById( 1 );

	NeighborhoodDatabase::RawDataset rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}
	}

	ASSERT_TRUE( entry.instances.empty() );

	entry.addInstance( std::move( rawDataset ) );

	ASSERT_EQ( 1, entry.instances.size() );
}

TEST( NeighborhoodDatabase_Query, all ) {
	NeighborhoodDatabase db;

	auto &entry = db.getEntryById( 1 );

	NeighborhoodDatabase::RawDataset rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}
	}

	entry.addInstance( std::move( NeighborhoodDatabase::RawDataset( rawDataset ) ) );

	NeighborhoodDatabase::Query query( db, 2.0, 20.0, std::move( rawDataset ) );

	const auto results = query.execute();

	ASSERT_EQ( 1, results.size() );
	EXPECT_FLOAT_EQ( 1.0f, results.front().first );
	EXPECT_EQ( 1, results.front().second );
}

//////////////////////////////////////////////////////////////////////////
// V2
// 

TEST( NeighborhoodDatabaseV2_SortedDataset, all ) {
	NeighborhoodDatabaseV2::RawDataset rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}
	}

	NeighborhoodDatabaseV2::SortedDataset sortedDataset( std::move( rawDataset ) );
	
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

	auto &a = db.getEntryById( 10 );
	auto &b = db.getEntryById( 10 );

	ASSERT_EQ( &a, &b );

	auto &c = db.getEntryById( 100 );

	ASSERT_NE( &b, &c );
}

TEST( NeighborhoodDatabaseV2_Entry, addInstance ) {
	NeighborhoodDatabaseV2 db;

	const auto &constEntry = db.getEntryById( 1 );

	NeighborhoodDatabaseV2::RawDataset rawDataset;

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

	NeighborhoodDatabaseV2::RawDataset rawDataset;

	for( int id = 100 - 1 ; id >= 0 ; id-- ) {
		for( int distance = 10 - 1 ; distance >= 0 ; distance-- ) {
			rawDataset.push_back( std::make_pair( id, float( distance ) ) );
		}

		ModelDatabase::ModelInformation info;
		info.area = info.diagonalLength = info.volume = 1.0;
		modelDatabase.informationById.emplace_back( std::move( info ) );
	}

	db.modelDatabase = &modelDatabase;

	db.addInstance( 1, std::move( NeighborhoodDatabaseV2::RawDataset( rawDataset ) ) );

	NeighborhoodDatabaseV2::Query query( db, 2.0, std::move( rawDataset ) );

	const auto results = query.execute();

	ASSERT_EQ( 1, results.size() );
	EXPECT_FLOAT_EQ( 1.0f, results.front().first );
	EXPECT_EQ( 1, results.front().second );
}