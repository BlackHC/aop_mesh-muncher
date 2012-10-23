#include "probeGenerator.h"

#include "gtest.h"

TEST( ProbeGenerator, uniqureDirections ) {
	ProbeGenerator::initDirections();
	for( int i = 0 ;  i < ProbeGenerator::getNumDirections() ; ++i ) {
		for( int j = 0 ; j < ProbeGenerator::getNumDirections() ; ++j ) {
			if( i == j ) {
				continue;
			}
			const float cosAngle = ProbeGenerator::getDirection( i ).dot( ProbeGenerator::getDirection( j ) );
			EXPECT_TRUE( cosAngle < 0.9 ) << i << " " << j << " " << cosAngle;
		}
	}
}

TEST( ProbeGenerator, uniqueOrientations ) {
	ProbeGenerator::initDirections();
	ProbeGenerator::initOrientations();

	for( int i = 0 ;  i < ProbeGenerator::getNumOrientations() ; ++i ) {
		for( int j = 0 ; j < ProbeGenerator::getNumOrientations() ; ++j ) {
			if( i == j ) {
				continue;
			}
			const float cosAngle = ( (ProbeGenerator::getRotation( i ) * ProbeGenerator::getRotation( j ).transpose()).trace() - 1.0f ) / 2.f;
			EXPECT_TRUE( cosAngle < 0.9 ) << i << " " << j << " " << cosAngle;
		}
	}
}

TEST( ProbeGenerator, bijectiveRotatedDirections ) {
	ProbeGenerator::initDirections();
	ProbeGenerator::initOrientations();

#if 0
	// this is the quick test
	for( int i = 0 ;  i < ProbeGenerator::getNumOrientations() ; ++i ) {
		const int *rotatedDirections = ProbeGenerator::getRotatedDirections( i );

		int sum = 0;
		int targetSum = 0;
		for( int j = 0 ; j < ProbeGenerator::getNumDirections() ; ++j ) {
			sum += rotatedDirections[ j ];
			targetSum += j;
		}
		ASSERT_EQ( sum, targetSum );
	}
#else
	ASSERT_EQ( 26, ProbeGenerator::getNumDirections() );
	for( int i = 0 ;  i < ProbeGenerator::getNumOrientations() ; ++i ) {
		const int *rotatedDirections = ProbeGenerator::getRotatedDirections( i );

		int hitVector[26] = { 0 };

		for( int j = 0 ; j < ProbeGenerator::getNumDirections() ; ++j ) {
			hitVector[ rotatedDirections[ j ] ]++;
		}
	
		for( int j = 0 ; j < ProbeGenerator::getNumDirections() ; ++j ) {
			EXPECT_EQ( 1, hitVector[ j ] ) << "orientation: " << i << ", direction " << j;
		}
	}
#endif
}