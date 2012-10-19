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
		return std::tanf( (float) PI_2 - radians );
	}
}

namespace Eigen {
	// like glFrustum
	inline Matrix4f createFrustumMatrix( const float left, const float right, const float bottom, const float top, const float zNear, const float zFar ) {
		const float width = right - left;
		const float height = top - bottom;
		const float depth = zFar - zNear;

		return (Matrix4f() <<
			2 * zNear / width,	0,					(right + left) / width,		0,
			0,					2 * zNear / height,	(top + bottom) / height,	0,
			0,					0,					-(zFar + zNear) / depth,	-2 * zFar * zNear / depth,
			0,					0,					-1.0,						0).finished();
	}

	// like glOrtho
	inline Matrix4f createOrthoProjectionMatrix( const float left, const float right, const float bottom, const float top, const float zNear, const float zFar ) {
		const float width = right - left;
		const float height = top - bottom;
		const float depth = zFar - zNear;

		return (Matrix4f() <<
			2 / width,		0,					0,				-(right + left) / width,
			0,				2 / height,			0,				-(top + bottom) / height,
			0,				0,					-2 / depth,		-(zFar + zNear) / depth,
			0,				0,					0,				1.0).finished();
	}

	// min_z = zNear, max_z = zFar
	// looks along positive z
	inline Matrix4f createOrthoProjectionMatrixLH( const Vector3f &min, const Vector3f &max ) {
		const Vector3f center = (min + max) / 2.0;
		const Vector3f halfSize = (max - min) / 2.0;

		return (Matrix4f() <<
			1.0f / halfSize.x(),	0,					0,						-center.x() / halfSize.x(),
			0,						1.0f / halfSize.y(),0,						-center.y() / halfSize.y(),
			0,						0,					1.0f / halfSize.z(),	-center.z() / halfSize.z(),
			0,						0,					0,						1.0f).finished();
	}

	typedef float Degrees;
	inline Matrix4f createPerspectiveProjectionMatrix( const Degrees FoV_y, const float aspectRatio, const float zNear, const float zFar ) {
		const float f = Math::cotf( FoV_y * 0.5f * (float) Math::PI / 180.0f );
		const float depth = zFar - zNear;

		return (Matrix4f() <<
			f / aspectRatio,	0, 0,						0,
			0,					f, 0,						0,
			0,					0, -(zFar + zNear) / depth,	-2 * zFar * zNear / depth,
			0,					0, -1.0,					0).finished();
	}

	// min_z = zNear, max_z = zFar
	// looks along positive z
	inline Matrix4f createShearProjectionMatrixLH( const Vector3f &min, const Vector3f &max, const Vector2f &zDirection ) {
		const Vector3f center = (min + max) / 2.0;
		const Vector3f halfSize = (max - min) / 2.0;

		return (Matrix4f() <<
			1.0f / halfSize.x(),	0,					-zDirection.x() / halfSize.x(),	-center.x() / halfSize.x(),
			0,						1.0f / halfSize.y(),-zDirection.y() / halfSize.y(),	-center.y() / halfSize.y(),
			0,						0,					zDirection.z() / halfSize.z(), -center.z() / halfSize.z(),
			0,						0,					0,							1.0f).finished();
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

	inline Matrix4f createViewerMatrixLH( const Vector3f &position, const Vector3f &forward, const Vector3f &up ) {
		const RowVector3f unitForward = forward.normalized();
		const RowVector3f right = forward.cross( up ).normalized();
		const RowVector3f orthogonalUp = right.cross( forward );

		Matrix3f view;
		view << right, orthogonalUp, unitForward;

		return (view * Translation3f( -position )).matrix();
	}

	// TODO: rename header to something more fitting.. eigenMatrixHelpers?
	inline Matrix4f createViewerMatrix( const Vector3f &position, const Vector3f &forward, const Vector3f &up ) {
		const RowVector3f unitForward = forward.normalized();
		const RowVector3f right = forward.cross( up ).normalized();
		const RowVector3f orthogonalUp = right.cross( forward );

		Matrix3f view;
		view << right, orthogonalUp, -unitForward;

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

	// TODO: add unit tests [9/30/2012 kirschan2]
	inline void unprojectAxes( const Vector3f &eye, const Matrix4f &projectionViewMatrix, Vector3f &u, Vector3f &v, Vector3f &w ) {
		Eigen::Matrix4f inverseProjectionView = projectionViewMatrix.inverse();

		// this works with all usual projection matrices (where x and y don't have any effect on z and w in clip space)
		// determine u, v, and w by unprojecting (x,y,-1,1) from clip space to world space
				
		// this is the w coordinate of the unprojected coordinates
		const float unprojectedW = inverseProjectionView(3,3) - inverseProjectionView(3,2);
		
		// divide the homogeneous affine matrix by the projected w
		// see R1 page for deduction
		Eigen::Matrix< float, 3, 4> inverseProjectionView34 = inverseProjectionView.topLeftCorner<3,4>() / unprojectedW;
		u = inverseProjectionView34.col(0);
		v = inverseProjectionView34.col(1);
		w = inverseProjectionView34.col(3) - inverseProjectionView34.col(2) - eye;
	}

	// TODO: add unit tests [9/30/2012 kirschan2]
	// z = -distance (RH)
	inline void unprojectAxes( const Vector3f &eye, const Matrix4f &projectionViewMatrix, float distance, Vector3f &u, Vector3f &v, Vector3f &w ) {
		Eigen::Matrix4f inverseProjectionView = projectionViewMatrix.inverse();

		// this works with all usual projection matrices (where x and y don't have any effect on z and w in clip space)
		// determine u, v, and w by unprojecting (x,y,-1,1) from clip space to world space

		// this is the w coordinate of the unprojected coordinates
		const float unprojectedW = inverseProjectionView(3,3) - distance * inverseProjectionView(3,2);

		// divide the homogeneous affine matrix by the projected w
		// see R1 page for deduction
		Eigen::Matrix< float, 3, 4> inverseProjectionView34 = inverseProjectionView.topLeftCorner<3,4>() / unprojectedW;
		u = inverseProjectionView34.col(0);
		v = inverseProjectionView34.col(1);
		w = inverseProjectionView34.col(3) - distance * inverseProjectionView34.col(2) - eye;
	}

	inline float distanceAlongAxis( const Eigen::Vector3f &v, const Eigen::Vector3f &axis ) {
		return axis.normalized().dot( v );
	}

	// projection matrix to frustum planes
	// the planes point inward, ie plane . (v 1) >= 0 for v inside the frustum
	typedef Matrix< float, 6, 4, RowMajor > FrustumPlanesMatrixf;
	const float projectionToFrustumPlanes_coeffs[] = {		
		1.0, 0.0, 0.0, 1.0, // left
		-1.0, 0.0, 0.0, 1.0, // right
		0.0, 1.0, 0.0, 1.0, // bottom
		0.0, -1.0, 0.0, 1.0, // top
		0.0, 0.0, 1.0, 1.0, // near
		0.0, 0.0, -1.0, 1.0  // far
	};
	enum FrustumPlaneIndices {
		FDI_LEFT,
		FDI_RIGHT,
		FDI_BOTTOM,
		FDI_TOP,
		FDI_NEAR,
		FDI_FAR
	};
	const Map< const FrustumPlanesMatrixf > projectionToFrustumPlanes( projectionToFrustumPlanes_coeffs );

	namespace Plane {
		inline RowVector4f parallelShift( const RowVector4f &plane, float distance ) {
			return plane - RowVector4f::UnitW() * distance * plane.head<3>().norm(); 
		}

		inline RowVector4f normalize( const RowVector4f &plane ) {
			return plane / plane.head<3>().norm(); 
		}

		inline RowVector4f parallelShiftNormalized( const RowVector4f &plane, float distance ) {
			return plane - RowVector4f::UnitW() * distance; 
		}
	}

	namespace Frustum {
		// minDistance is only length invariant if the frustum planes are normalized
		// minDistance > 0 makes the frustum smaller
		// minDistance < 0 makes the frustum bigger
		template< typename Derived >
		inline bool isInside( const MatrixBase< Derived > &frustumPlanes, const Vector3f &point, float minDistance = 0.0f ) {
			return ((frustumPlanes * point.homogeneous()).array() >= minDistance).all();
		}

		template< typename Derived >
		inline bool isInside( const MatrixBase< Derived > &frustumPlanes, const Vector4f &point, float minDistance = 0.0f  ) {
			return ((frustumPlanes * point).cwise() >= minDistance).all();
		}

		inline FrustumPlanesMatrixf normalize( const FrustumPlanesMatrixf &frustumPlanes ) {
			FrustumPlanesMatrixf normalized;
			for( int i = 0 ; i < 6 ; ++i ) {
				normalized.row(i) = Plane::normalize( frustumPlanes.row(i) );	
			}
			return normalized;
		}
	}

	inline Vector3f getPerpendicular( const Vector3f &v ) {
		// TODO: fix the zero test [9/20/2012 kirschan2]
		if( abs( v.x() ) < 0.0001 ) {
			return Vector3f( 0.0f, -v.z(), v.y() );
		}
		else if( abs( v.y() ) < 0.0001 ) {
			return Vector3f( -v.z(), 0.0f, v.x() );
		}
		else {
			return Vector3f( v.y(), -v.x(), 0.0f );
		}
	}

	namespace OrthonormalBase {
		inline Matrix3f fromNormal( const Vector3f &normal ) {
			Matrix3f base;
			base.col(0) = normal.normalized();
			base.col(1) = getPerpendicular( base.col(0) ).normalized();
			base.col(2) = base.col(0).cross( base.col(1) );
			return base;
		}
	}
}