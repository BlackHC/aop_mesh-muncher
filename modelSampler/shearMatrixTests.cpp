#include <Eigen/Eigen>
#include "camera.h"

#include "gtest.h"

using namespace Eigen;

std::ostream & operator << (std::ostream & s, const Matrix4f & m) {
	return Eigen::operator<<( s, m );
}

std::ostream & operator << (std::ostream & s, const Vector4f & m) {
	return Eigen::operator<<( s, m );
}

TEST( ShearMatrixTests, noShearUniform) {
	Matrix4f result = createShearProjectionMatrix( Vector2f::Constant(-1.0), Vector2f::Constant(1.0), -1.0, 1.0, Vector2f::Zero() );
	Matrix4f expected = Eigen::createOrthoProjectionMatrix(  Vector3f::Constant(-1.0), Vector3f::Constant(1.0) );
	ASSERT_EQ( result, expected );
}

TEST( ShearMatrixTests, noShear) {
	Matrix4f result = createShearProjectionMatrix( Vector2f::Constant(-5.0), Vector2f::Constant(1.0), -5.0, 1.0, Vector2f::Zero() );
	Matrix4f expected = Eigen::createOrthoProjectionMatrix( Vector3f::Constant(-5.0), Vector3f::Constant(1.0) );
	ASSERT_EQ( result, expected );
}

TEST( ShearMatrixTests, xShear) {
	Matrix4f shearMatrix = createShearProjectionMatrix( Vector2f::Constant(-1.0), Vector2f::Constant(1.0), -1.0, 1.0, Vector2f( 1.0, 0.0 ) );
	
	EXPECT_EQ( Vector4f(shearMatrix * Vector4f::UnitW()), Vector4f::UnitW() );
	EXPECT_EQ( Vector4f(shearMatrix * Vector4f( 0.0, 0.0, -1.0, 1.0 )), Vector4f( 1.0, 0.0, 1.0, 1.0 ) );
}

TEST( ShearMatrixTests, yShear) {
	Matrix4f shearMatrix = createShearProjectionMatrix( Vector2f::Constant(-1.0), Vector2f::Constant(1.0), -1.0, 1.0, Vector2f( 0.0, 1.0 ) );

	EXPECT_EQ( Vector4f(shearMatrix * Vector4f::UnitW()), Vector4f::UnitW() );
	EXPECT_EQ( Vector4f(shearMatrix * Vector4f( 0.0, 0.0, -1.0, 1.0 )), Vector4f( 0.0, 1.0, 1.0, 1.0 ) );
}