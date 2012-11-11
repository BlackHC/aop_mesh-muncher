#include "flatImmutableMultiMap.h"

#include <gtest.h>

TEST( FlatImmutableOrderedMultiMap, init ) {
	FlatImmutableOrderedMultiMap< int, int > map;
	map.init( 10 );
	ASSERT_EQ( 11, map.bucketOffsets.size() );
}

TEST( FlatImmutableOrderedMultiMap, buildSimple ) {
	FlatImmutableOrderedMultiMap< int, int > map;
	map.init( 10 );
	ASSERT_EQ( 11, map.bucketOffsets.size() );

	auto builder = map.createBuilder();
	for( int i = 0 ; i < 100 ; i++ ) {
		builder.push_back( i, i );
	}

	builder.build();

	for( int i = 0 ; i < 100 ; i++ ) {
		auto range = map.equal_range( i );
		ASSERT_EQ( 1, range.second - range.first );

		ASSERT_EQ( i, range.first->first );
		ASSERT_EQ( i, range.first->second );
	}
}

TEST( FlatImmutableOrderedMultiMap, buildMultiple ) {
	FlatImmutableOrderedMultiMap< int, int > map;
	map.init( 10 );
	ASSERT_EQ( 11, map.bucketOffsets.size() );

	auto builder = map.createBuilder();
	for( int i = 0 ; i < 100 ; i++ ) {
		builder.push_back( i, i + 200 );
		builder.push_back( i, i + 100 );
		builder.push_back( i, i );
	}

	builder.build();

	for( int i = 0 ; i < 100 ; i++ ) {
		auto range = map.equal_range( i );
		ASSERT_EQ( 3, range.second - range.first );

		for( int j = 0 ; j < 3 ; j++ ) {
			EXPECT_EQ( i, range.first[j].first );	
			EXPECT_EQ( i + j*100, range.first[j].second );
		}
	}
}