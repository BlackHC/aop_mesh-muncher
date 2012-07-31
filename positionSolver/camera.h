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
		return std::tanf( PI_2 - radians );
	}
}

namespace Eigen {
	// like glFrustum
	Matrix4f createFrustumMatrix( const float left, const float right, const float bottom, const float top, const float near, const float far ) {
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
	Matrix4f createOrthoMatrix( const float left, const float right, const float bottom, const float top, const float near, const float far ) {
		const float width = right - left;
		const float height = top - bottom;
		const float depth = far - near;

		return (Matrix4f() <<
			2 / width,		0,					0,				(right + left) / width,
			0,				2 / height,			0,				(top + bottom) / height,
			0,				0,					-2 / depth,		-(far + near) / depth,
			0,				0,					0,				-1.0).finished();
	}

	Matrix4f createPerspectiveMatrix( const float FoV_y, const float aspectRatio, const float zNear, const float zFar ) {
		const float f = Math::cotf( FoV_y * M_PI / 180 / 2 );
		const float depth = zFar - zNear;

		return (Matrix4f() <<
			f * aspectRatio,	0, 0,						0,
			0,					f, 0,						0,
			0,					0, -(zFar + zNear) / depth,	-2 * zFar * zNear / depth,
			0,					0, -1.0,					0).finished();
	}
}

struct Camera {
	const Eigen::Vector3f &getPosition() const { return position; }
	void setPosition(const Eigen::Vector3f &val) { position = val; }

	const Eigen::Quaternionf &getOrientation() const { return orientation; }
	void setOrientation(const Eigen::Quaternionf &val) { orientation = val; }

	Eigen::Matrix4f getViewMatrix() const {
		Eigen::Isometry3f viewTransformation = orientation * Eigen::Translation3f( -position );
		return viewTransformation.matrix();	
	}

	Eigen::Isometry3f getViewTransformation() const {
		Eigen::Isometry3f viewTransformation = orientation * Eigen::Translation3f( -position );
		return viewTransformation;
	}

	struct PerspectiveProjectionParameters {
		float FoV_y;
		float aspect;
		float zNear, zFar;
	};
	PerspectiveProjectionParameters perspectiveProjectionParameters;

	Eigen::Matrix4f getProjectionMatrix() const {
		return createPerspectiveMatrix( 
			perspectiveProjectionParameters.FoV_y,
			perspectiveProjectionParameters.aspect,
			perspectiveProjectionParameters.zNear,
			perspectiveProjectionParameters.zFar
			);
	}

	Camera() : position( Eigen::Vector3f::Zero() ), orientation( Eigen::Quaternionf::Identity() ) {}

	Eigen::Vector3f position;
	Eigen::Quaternionf orientation;
private:
};