#include "grid.h"

#include "gtest.h"

using namespace Eigen;

static std::ostream & operator << (std::ostream & s, const Matrix4f & m) {
	return Eigen::operator<<( s, m );
}

static std::ostream & operator << (std::ostream & s, const Vector4f & m) {
	return Eigen::operator<<( s, m );
}

TEST( OrientedGrid, fromIdentity) {
	OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(4), Vector3f::Zero(), 1.0 );
	EXPECT_EQ( grid.transformation.matrix(), Matrix4f::Identity() );
	EXPECT_EQ( grid.getPosition( Vector3i::Zero() ), Vector3f::Zero() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitX() ), Vector3f::UnitX() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitY() ), Vector3f::UnitY() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitZ() ), Vector3f::UnitZ() );
}

TEST( OrientedGrid, fromIdentityWithOffset) {
	const Vector3f offset = Vector3f::Constant(1.0);
	const OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(4), offset, 1.0 );
	EXPECT_EQ( grid.getPosition( Vector3i::Zero() ), offset );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitX() ), offset + Vector3f::UnitX() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitY() ), offset + Vector3f::UnitY() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitZ() ), offset + Vector3f::UnitZ() );
}

TEST( OrientedGrid, fromOffsetAndResolution) {
	const Vector3f offset = Vector3f::Constant(1.0);
	const OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(4), offset, 2.0 );
	EXPECT_EQ( grid.getPosition( Vector3i::Zero() ), offset );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitX() ), offset + Vector3f::UnitX() * 2 );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitY() ), offset + Vector3f::UnitY() * 2 );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitZ() ), offset + Vector3f::UnitZ() * 2 );
}

TEST( OrientedGrid, permuted ) {
	const int permutation[] = {1,2,0};
	const Vector3f offset = Vector3f::Constant(1.0);
	const OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(4), offset, 2.0 );
	const OrientedGrid permutedGrid = OrientedGrid::from( grid, permutation );

	EXPECT_EQ( permutedGrid.getPosition( Vector3i::Zero() ), offset );
	EXPECT_EQ( permutedGrid.getPosition( Vector3i::UnitZ() ), offset + Vector3f::UnitX() * 2 );
	EXPECT_EQ( permutedGrid.getPosition( Vector3i::UnitX() ), offset + Vector3f::UnitY() * 2 );
	EXPECT_EQ( permutedGrid.getPosition( Vector3i::UnitY() ), offset + Vector3f::UnitZ() * 2 );
}

static Vector4f h( const Vector3f &v ) {
	return (Vector4f() << v, 1.0).finished();
}

TEST( OrientedGrid, permuted_inverse ) {
	const int permutation[] = {1,2,0};
	const Vector3f offset = Vector3f::Constant(1.0);
	const OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(4), offset, 2.0 );
	const OrientedGrid permutedGrid = OrientedGrid::from( grid, permutation );

	const Matrix4f inv = permutedGrid.transformation.inverse().matrix();
	EXPECT_EQ( h( Vector3f::Zero() ), inv * h(offset) );
	EXPECT_EQ( h( Vector3f::UnitZ() ), inv * h(offset + Vector3f::UnitX() * 2) );
	EXPECT_EQ( h( Vector3f::UnitX() ), inv * h(offset + Vector3f::UnitY() * 2) );
	EXPECT_EQ( h( Vector3f::UnitY() ), inv * h(offset + Vector3f::UnitZ() * 2) );
}
