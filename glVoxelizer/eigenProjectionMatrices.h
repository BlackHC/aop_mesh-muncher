#pragma once

#include <Eigen/Eigen>

namespace Math {
	const double E = 2.71828182845904523536;
	const double LOG2E = 1.44269504088896340736;
	const double LOG10E = 0.434294481903251827651;
	const double LN2 = 0.693147180559945309417;
	const double LN10 = 2.30258509299404568402;
	const double PI = 3.14159265358979323846;
	const double PI_2 = 1.57079632679489661923;
	const double PI_4 = 0.785398163397448309616;
	const double INV_PI = 0.318309886183790671538;
	const double INV_PI_2 = 0.636619772367581343076;
	const double SQRT2 = 1.41421356237309504880;
	const double SQRT1_2 = 0.707106781186547524401;

	inline float cotf( float radians ) {
		return std::tanf( float(PI_2) - radians );
	}
}

namespace Eigen {
	// like glFrustum
	inline Matrix4f createFrustumMatrix( const float left, const float right, const float bottom, const float top, const float near, const float far ) {
		const float width = right - left;
		const float height = top - bottom;
		const float depth = far - near;

		return (Matrix4f() <<
			2 * near / width,	0,					(right + left) / width,		0,
			0,					2 * near / height,	(top + bottom) / height,	0,
			0,					0,					-(far + near) / depth,		-2 * far * near / depth,
			0,					0,					-1.0,						0).finished();
	}

	// like glOrtho
	inline Matrix4f createOrthoProjectionMatrix( const float left, const float right, const float bottom, const float top, const float near, const float far ) {
		const float width = right - left;
		const float height = top - bottom;
		const float depth = far - near;

		return (Matrix4f() <<
			2 / width,		0,					0,				-(right + left) / width,
			0,				2 / height,			0,				-(top + bottom) / height,
			0,				0,					-2 / depth,		-(far + near) / depth,
			0,				0,					0,				1.0).finished();
	}


	inline Matrix4f createOrthoProjectionMatrix( const Vector2f &min, const Vector2f &max, const float zNear, const float zFar ) {
		const Vector2f center = (min + max) / 2.0;
		const Vector2f halfSize = (max - min) / 2.0;
		const float depth = zFar - zNear;

		return (Matrix4f() <<
			1.0f / halfSize.x(),	0,					0,					-center.x() / halfSize.x(),
			0,						1.0f / halfSize.y(),0,					-center.y() / halfSize.y(),
			0,						0,					-2.0f / depth,		-(zFar + zNear) / depth,
			0,						0,					0,					1.0f).finished();
	}

	inline Matrix4f createOrthoProjectionMatrixLH( const Vector2f &min, const Vector2f &max, const float zNear, const float zFar ) {
		const Vector2f center = (min + max) / 2.0;
		const Vector2f halfSize = (max - min) / 2.0;
		const float depth = zFar - zNear;

		return (Matrix4f() <<
			1.0f / halfSize.x(),	0,					0,					-center.x() / halfSize.x(),
			0,						1.0f / halfSize.y(),0,					-center.y() / halfSize.y(),
			0,						0,					2.0f / depth,		-(zFar + zNear) / depth,
			0,						0,					0,					1.0f).finished();
	}

	typedef float Degrees;
	inline Matrix4f createPerspectiveProjectionMatrix( const Degrees FoV_y, const float aspectRatio, const float zNear, const float zFar ) {
		const float f = Math::cotf( FoV_y * float(Math::PI) / 180 / 2 );
		const float depth = zFar - zNear;

		return (Matrix4f() <<
			f / aspectRatio,	0, 0,						0,
			0,					f, 0,						0,
			0,					0, -(zFar + zNear) / depth,	-2 * zFar * zNear / depth,
			0,					0, -1.0,					0).finished();
	}

	// also looks down the negative Z axis!
	inline Matrix4f createShearProjectionMatrix( const Vector2f &min, const Vector2f &max, const float zNear, const float zFar, const Vector2f &zStep ) {
		const Vector2f center = (min + max) / 2.0;
		const Vector2f halfSize = (max - min) / 2.0;
		const float depth = zFar - zNear;

		return (Matrix4f() <<
			1.0f / halfSize.x(),	0,					zStep.x() / halfSize.x(),	-center.x() / halfSize.x(),
			0,						1.0f / halfSize.y(),zStep.y() / halfSize.y(),	-center.y() / halfSize.y(),
			0,						0,					-2.0f / depth,				-(zFar + zNear) / depth,
			0,						0,					0,							1.0f).finished();
	}

	// TODO: rename header to something more fitting.. eigenMatrixHelpers?
	static Matrix4f createViewerMatrix( const Vector3f &position, const Vector3f &forward, const Vector3f &up ) {
		const RowVector3f right = forward.cross( up ).normalized();
		const RowVector3f realUp = right.cross( forward );

		Matrix3f view;
		view << right, realUp, -forward.transpose();

		return (view * Translation3f( -position )).matrix();
	}

	inline Matrix4f createLookAtMatrix( const Vector3f &position, const Vector3f &point, const Vector3f &up ) {
		const RowVector3f forward = (point - position).normalized();
		const RowVector3f right = forward.cross( up ).normalized();
		const RowVector3f realUp = right.cross( forward );

		Matrix3f view;
		view << right, realUp, -forward;
		
		return (view * Translation3f( -position )).matrix();
	}
}
