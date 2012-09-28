#include "mathUtility.h"

#include <gtest.h>

using namespace Eigen;

TEST(rayIntersections_unitCube, mainAxis ) {
	Vector3f hitPoint;
	EXPECT_TRUE( intersectRayWithUnitCube( Vector3f::UnitX() * 2, -Vector3f::UnitX(), hitPoint ) );
	EXPECT_TRUE( intersectRayWithUnitCube( Vector3f::UnitY() * 2, -Vector3f::UnitY(), hitPoint ) );
	EXPECT_TRUE( intersectRayWithUnitCube( Vector3f::UnitZ() * 2, -Vector3f::UnitZ(), hitPoint ) );

	EXPECT_TRUE( intersectRayWithUnitCube( -Vector3f::UnitX() * 2, Vector3f::UnitX(), hitPoint ) );
	EXPECT_TRUE( intersectRayWithUnitCube( -Vector3f::UnitY() * 2, Vector3f::UnitY(), hitPoint ) );
	EXPECT_TRUE( intersectRayWithUnitCube( -Vector3f::UnitZ() * 2, Vector3f::UnitZ(), hitPoint ) );

	// false: x
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitX() * 2, Vector3f::UnitY(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitX() * 2, Vector3f::UnitZ(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitX() * 2, -Vector3f::UnitY(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitX() * 2, -Vector3f::UnitZ(), hitPoint ) );

	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitX() * 2, Vector3f::UnitY(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitX() * 2, Vector3f::UnitZ(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitX() * 2, -Vector3f::UnitY(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitX() * 2, -Vector3f::UnitZ(), hitPoint ) );

	// false: y
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitY() * 2, Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitY() * 2, Vector3f::UnitZ(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitY() * 2, -Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitY() * 2, -Vector3f::UnitZ(), hitPoint ) );

	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitY() * 2, Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitY() * 2, Vector3f::UnitZ(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitY() * 2, -Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitY() * 2, -Vector3f::UnitZ(), hitPoint ) );

	// false: z
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitZ() * 2, Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitZ() * 2, Vector3f::UnitY(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitZ() * 2, -Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( Vector3f::UnitZ() * 2, -Vector3f::UnitY(), hitPoint ) );

	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitZ() * 2, Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitZ() * 2, Vector3f::UnitY(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitZ() * 2, -Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithUnitCube( -Vector3f::UnitZ() * 2, -Vector3f::UnitY(), hitPoint ) );
}

#if 0
TEST(rayIntersections_unitCube, hitPointCheck ) {
	for( int ix = -100 ; ix <= 100 ; ++ix )
		for( int iy = -100 ; iy <= 100 ; ++iy )
			for( int iz = -100 ; iz <= 100 ; ++iz ) {
				const Vector3f position( ix / 10.0, iy / 10.0, iz / 10.0 );

				Vector3f hitPoint;
				EXPECT_TRUE(  intersectRayWithUnitCube( position, -position, hitPoint ) );

				EXPECT_TRUE( hitPoint.cwiseAbs().maxCoeff() < 1.1f );
			}
}
#endif

TEST(rayIntersections_box, mainAxis ) {
	
	const Vector3f offset(100, 50, -75 );
	const Vector3f size(8, 4, 2);
	const AlignedBox3f box( offset - size / 2, offset + size / 2 );

	Vector3f hitPoint;
	// xy
	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 5, 0, 0 ), -Vector3f::UnitX(), hitPoint ) );
	
	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 5, 1, 0 ), -Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithAABB( box, offset + Vector3f( 5, 3, 0 ), -Vector3f::UnitX(), hitPoint ) );

	// xz
	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 5, 0, 0 ), -Vector3f::UnitX(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithAABB( box, offset + Vector3f( 5, 0, 2 ), -Vector3f::UnitX(), hitPoint ) );

	// yx
	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 0, 5, 0 ), -Vector3f::UnitY(), hitPoint ) );

	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 3, 5, 0 ), -Vector3f::UnitY(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithAABB( box, offset + Vector3f( 5, 5, 0 ), -Vector3f::UnitY(), hitPoint ) );

	// yz
	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 0, 5, 0 ), -Vector3f::UnitY(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithAABB( box, offset + Vector3f( 0, 5, 2 ), -Vector3f::UnitY(), hitPoint ) );

	// zx
	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 0, 0, 5 ), -Vector3f::UnitZ(), hitPoint ) );

	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 3, 0, 5 ), -Vector3f::UnitZ(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithAABB( box, offset + Vector3f( 5, 0, 5 ), -Vector3f::UnitZ(), hitPoint ) );

	// zy
	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 0, 0, 5 ), -Vector3f::UnitZ(), hitPoint ) );

	EXPECT_TRUE( intersectRayWithAABB( box, offset + Vector3f( 0, 1, 5 ), -Vector3f::UnitZ(), hitPoint ) );
	EXPECT_FALSE( intersectRayWithAABB( box, offset + Vector3f( 0, 3, 5 ), -Vector3f::UnitZ(), hitPoint ) );
}

// a few tests for unproject
#include "eigenProjectionMatrices.h"

TEST( unprojectAxes, eye ) {
	const Matrix4f projection = createPerspectiveProjectionMatrix( 90.0, 1.0, 1.0, 100.0 );

	Vector3f eye;
	Vector3f dummy;
	
	unprojectAxesAndOrigin( projection, dummy, dummy, dummy, eye );
	EXPECT_TRUE( eye.isZero() );

	Vector3f realEye = Vector3f( 10, 50, 100 );
	unprojectAxesAndOrigin( (Projective3f( projection ) * Translation3f( realEye )).matrix(), dummy, dummy, dummy, eye );
	EXPECT_TRUE( eye.isApprox( realEye ) );
}