#pragma once

#include <Eigen/Eigen>

struct OBB {
	typedef Eigen::Affine3f Transformation;
	// origin box to world (without scaling)
	Transformation transformation;
	Eigen::Vector3f size;
};

// z: 9, x: 9, y: 8
const Eigen::Vector3i neighborOffsets[] = {
	// first z, then x, then y

	// z
	Eigen::Vector3i( 0, 0, 1 ), Eigen::Vector3i( 0, 0, -1 ), Eigen::Vector3i( -1, 0, -1 ),
	Eigen::Vector3i( 1, 0, -1 ), Eigen::Vector3i( -1, 0, 1 ),  Eigen::Vector3i( 1, 0, 1 ),
	Eigen::Vector3i( 0, 1, -1 ), Eigen::Vector3i( 0, -1, -1 ), Eigen::Vector3i( -1, 1, -1 ),
	// x
	Eigen::Vector3i( 1, 0, 0 ), Eigen::Vector3i( -1, 0, 0  ), Eigen::Vector3i( 1, 1, -1 ),
	Eigen::Vector3i( 1, -1, 1 ), Eigen::Vector3i( 1, 1, 1 ), Eigen::Vector3i( 1, -1, -1 ),
	Eigen::Vector3i( -1, 1, 0 ), Eigen::Vector3i( 1, 1, 0 ), Eigen::Vector3i( -1, -1, -1 ),
	// y
	Eigen::Vector3i( 0, 1, 0 ), Eigen::Vector3i( 0, -1, 0 ), Eigen::Vector3i( -1, -1, 0 ), Eigen::Vector3i( 1, -1, 0 ),
	Eigen::Vector3i( -1, -1, 1 ), Eigen::Vector3i( 0, -1, 1 ),  Eigen::Vector3i( -1, 1, 1 ), Eigen::Vector3i( 0, 1, 1 )
};

inline Eigen::Vector3i ceil( const Eigen::Vector3f &v );
inline Eigen::Vector3i floor( const Eigen::Vector3f &v );

template< typename Vector > Vector permute( const Vector &v, const int *permutation );
template< typename Vector > Vector permute_reverse( const Vector &w, const int *permutation );
inline Eigen::Matrix4f permutedToUnpermutedMatrix( const int *permutation );
inline Eigen::Matrix4f unpermutedToPermutedMatrix( const int *permutation );

OBB makeOBB( const Eigen::Matrix4f &transformation, const Eigen::AlignedBox3f &alignedBox );

const Eigen::Vector3f flipSign( const Eigen::Vector3f &v, const Eigen::Vector3f &c );


const Eigen::Vector3i indexToCubeCorner[] = {
	Eigen::Vector3i( 0,0,0 ), Eigen::Vector3i( 1,0,0 ), Eigen::Vector3i( 0,1,0 ), Eigen::Vector3i( 0,0,1 ),
	Eigen::Vector3i( 1,1,0 ), Eigen::Vector3i( 0,1,1 ), Eigen::Vector3i( 1,0,1 ), Eigen::Vector3i( 1,1,1 )
};

inline float squaredMinDistanceAABoxPoint( const Eigen::Vector3f &min, const Eigen::Vector3f &max, const Eigen::Vector3f &point ) {
	Eigen::Vector3f distance;
	for( int i = 0 ; i < 3 ; i++ ) {
		if( point[i] > max[i] ) {
			distance[i] = point[i] - max[i];
		}
		else if( point[i] > min[i] ) {
			distance[i] = 0.f;
		}
		else {
			distance[i] = min[i] - point[i];
		}
	}
	return distance.squaredNorm();
}

inline float squaredMaxDistanceAABoxPoint( const Eigen::Vector3f &min, const Eigen::Vector3f &max, const Eigen::Vector3f &point ) {
	Eigen::Vector3f distanceA = (point - min).cwiseAbs();
	Eigen::Vector3f distanceB = (point - max).cwiseAbs();
	Eigen::Vector3f distance = distanceA.cwiseMax( distanceB );

	return distance.squaredNorm();
}

inline Eigen::Vector3f nearestPointOnAABoxToPoint( const Eigen::Vector3f &min, const Eigen::Vector3f &max, const Eigen::Vector3f &point ) {
	Eigen::Vector3f nearestPoint;
	for( int i = 0 ; i < 3 ; i++ ) {
		if( point[i] > max[i] ) {
			nearestPoint[i] = max[i];
		}
		else if( point[i] > min[i] ) {
			nearestPoint[i] = point[i];
		}
		else {
			nearestPoint[i] = min[i];
		}
	}
	return nearestPoint;
}

inline Eigen::Vector3f farthestPointOnAABoxToPoint( const Eigen::Vector3f &min, const Eigen::Vector3f &max, const Eigen::Vector3f &point ) {
	Eigen::Vector3f farthestPoint;
	for( int i = 0 ; i < 3 ; i++ ) {
		if( fabs( point[i] - min[i] ) > fabs( point[i] - max[i] ) ) {
			farthestPoint[i] = min[i];
		}
		else {
			farthestPoint[i] = max[i];
		}
	}
	return farthestPoint;
}

// unit cube is [-1,1]**3
bool intersectRayWithUnitCube( const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f &hitPoint );
bool intersectRayWithAABB( const Eigen::AlignedBox3f &box, const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f &hitPoint );

inline const Eigen::Vector3f flipSign( const Eigen::Vector3f &v, const Eigen::Vector3f &c ) {
	return Eigen::Vector3f( 
		(c[0] > 0.0) ? v[0] : -v[0],
		(c[1] > 0.0) ? v[1] : -v[1],
		(c[2] > 0.0) ? v[2] : -v[2]
	);
}

inline bool intersectRayWithUnitCube( const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f &hitPoint ) {
	using namespace Eigen;

	// we use the symmetry around the origin
	// put position in the first octant
	const Vector3f symPosition = flipSign( position, position );
	const Vector3f symDirection = flipSign( direction, position );

	const float epsilon = 0.000005;
	const float OneEpsilon = 1.000005;

	if( symPosition.maxCoeff() < OneEpsilon ) {
		hitPoint = position;
		return true;
	}

	// only need to check 3 possible intersection planes (those with positive normal)
	float t[3];
	for( int i = 0 ; i < 3 ; i++ ) {
		if( symDirection[i] != 0.0 ) {
			t[i] = (1.0 - symPosition[i]) / symDirection[i];
		}
		else {
			t[i] = -1.0;
		}
	}

	if( t[0] >= 0.0f ) {
		hitPoint = position + direction * t[0];
		if( fabs( hitPoint[1] ) < OneEpsilon && fabs( hitPoint[2] ) < OneEpsilon ) {
			return true;
		}
	}

	if( t[1] >= 0.0f ) {
		hitPoint = position + direction * t[1];
		if( fabs( hitPoint[0] ) < OneEpsilon && fabs( hitPoint[2] ) < OneEpsilon ) {
			return true;
		}
	}

	if( t[2] >= 0.0f ) {
		hitPoint = position + direction * t[2];
		if( fabs( hitPoint[0] ) < OneEpsilon && fabs( hitPoint[1] ) < OneEpsilon) {
			return true;
		}
	}

	return false;
}

inline bool intersectRayWithAABB( const Eigen::AlignedBox3f &box, const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f &hitPoint ) {
	using namespace Eigen;

	// box to world
	const auto transformation = Translation3f( box.center() ) * Scaling( box.sizes() / 2 );
	const auto invTransformation = Scaling( box.sizes() / 2 ).inverse() * Translation3f( -box.center() );
	const Vector3f transformedDirection = invTransformation.linear() * direction;
	const Vector3f transformedPosition = invTransformation * position;

	if( intersectRayWithUnitCube( transformedPosition, transformedDirection, hitPoint ) ) {
		// transform back
		hitPoint = transformation * hitPoint;
		return true;
	}
	return false;
}

inline bool intersectRayWithOBB( const OBB &obb, const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f &hitPoint ) {
	using namespace Eigen;

	// box to world
	const auto transformation = obb.transformation * Scaling( obb.size / 2 );
	const auto invTransformation = Scaling( obb.size / 2 ).inverse() * obb.transformation.inverse();
	const Vector3f transformedDirection = invTransformation.linear() * direction;
	const Vector3f transformedPosition = invTransformation * position;

	if( intersectRayWithUnitCube( transformedPosition, transformedDirection, hitPoint ) ) {
		// transform back
		hitPoint = transformation * hitPoint;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// implementation
// 
inline Eigen::Vector3i ceil( const Eigen::Vector3f &v ) {
	return Eigen::Vector3i( (int) ceil( v[0] ), (int) ceil( v[1] ), (int) ceil( v[2] ) );
}

inline Eigen::Vector3i floor( const Eigen::Vector3f &v ) {
	return Eigen::Vector3i( (int) floor( v[0] ), (int) floor( v[1] ), (int) floor( v[2] ) );
}

// xyz -120-> yzx
template< typename Vector >
Vector permute( const Vector &v, const int *permutation ) {
	return Vector( v[permutation[0]], v[permutation[1]], v[permutation[2]] );
}

// yzx -120->xyz
template< typename Vector >
Vector permute_reverse( const Vector &w, const int *permutation ) {
	Vector v;
	for( int i = 0 ; i < 3 ; ++i ) {
		v[ permutation[i] ] = w[i];	
	}
	return v;
}

inline Eigen::Matrix4f permutedToUnpermutedMatrix( const int *permutation ) {
	return (Eigen::Matrix4f() << Eigen::Vector3f::Unit( permutation[0] ), Eigen::Vector3f::Unit( permutation[1] ), Eigen::Vector3f::Unit( permutation[2] ), Eigen::Vector3f::Zero(), 0,0,0,1.0 ).finished();
}

inline Eigen::Matrix4f unpermutedToPermutedMatrix( const int *permutation ) {
	return (Eigen::Matrix4f() << Eigen::RowVector3f::Unit( permutation[0] ), 0.0,
		Eigen::RowVector3f::Unit( permutation[1] ), 0.0,
		Eigen::RowVector3f::Unit( permutation[2] ), 0.0,
		Eigen::RowVector4f::UnitW() ).finished();
}

inline OBB makeOBB( const Eigen::Matrix4f &transformation, const Eigen::AlignedBox3f &alignedBox ) {
	OBB obb;
	obb.transformation = Eigen::Affine3f( transformation ) * Eigen::Translation3f( alignedBox.center() );
	obb.size = alignedBox.sizes();
	return obb;
}
