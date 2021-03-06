#pragma once

#include <Eigen/Eigen>

// TODO: cleanup declarations and definitions etc! [10/19/2012 kirschan2]

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

template< typename Scalar >
Scalar clamp( const Scalar &value, const Scalar &min, const Scalar &max ) {
	if( value < min ) {
		return min;
	}
	else if( value > max ) {
		return max;
	}
	return value;
}

// alpha is the base probability for either event happening (this is for a two-event state)
inline float getMessageLength( float probability ) {
	const float messageLength = -logf( probability );

	return messageLength;
}

inline float getBinaryEntropy( float probability ) {
	const float positiveMessageLength = getMessageLength( probability );
	const float negativeMessageLength = getMessageLength( 1.0f - probability );

	const float binaryEntropy = positiveMessageLength * probability + (1.0f - probability) * negativeMessageLength;
	return binaryEntropy;
}

namespace ColorConversion {
	namespace HSV {
		/*
	    typedef struct {
			double r;       // percent
			double g;       // percent
			double b;       // percent
		} rgb;

			typedef struct {
			double h;       // angle in degrees
			double s;       // percent
			double v;       // percent
		} hsv;*/

		inline Eigen::Vector3f rgb2hsv(const Eigen::Vector3f &rgb)
		{
			Eigen::Vector3f hsv;

			const float min = rgb.minCoeff();
			const float max = rgb.maxCoeff();

			hsv[2] = max; // v
			const float delta = max - min;
			if( max > 0.0f ) {
				hsv[1] = delta / max; // s
			} else {
				// r = g = b = 0
				// s = 0, v is undefined
				hsv[1] = 0.0f;
				hsv[0] = 0.0f;
				return hsv;
			}
			if( rgb[0] >= max ) // > is bogus, just keeps compilor happy
				hsv[0] = ( rgb[1] - rgb[2] ) / delta;        // between yellow & magenta
			else
			if( rgb[1] >= max )
				hsv[0] = 2.0f + ( rgb[2] - rgb[0] ) / delta;  // between cyan & yellow
			else
				hsv[0] = 4.0f + ( rgb[0] - rgb[1] ) / delta;  // between magenta & cyan

			hsv[0] *= 60.0f;                              // degrees

			if( hsv[0] < 0.0 )
				hsv[0] += 360.0f;

			return hsv;
		}


		inline Eigen::Vector3f hsv2rgb( const Eigen::Vector3f &hsv )
		{
			float hh, p, q, t, ff;
			long i;
			Eigen::Vector3f rgb;

			if(hsv[1] <= 0.0f) {       // < is bogus, just shuts up warnings
				rgb[0] = 0.0f;
				rgb[1] = 0.0f;
				rgb[2] = 0.0f;
				return rgb;
			}
			hh = hsv[0];
			if(hh >= 360.0f) hh = 0.0f;
			hh /= 60.0f;
			i = (long)hh;
			ff = hh - i;
			p = hsv[2] * (1.0f - hsv[1]);
			q = hsv[2] * (1.0f - (hsv[1] * ff));
			t = hsv[2] * (1.0f - (hsv[1] * (1.0f - ff)));

			switch(i) {
			case 0:
				rgb[0] = hsv[2];
				rgb[1] = t;
				rgb[2] = p;
				break;
			case 1:
				rgb[0] = q;
				rgb[1] = hsv[2];
				rgb[2] = p;
				break;
			case 2:
				rgb[0] = p;
				rgb[1] = hsv[2];
				rgb[2] = t;
				break;

			case 3:
				rgb[0] = p;
				rgb[1] = q;
				rgb[2] = hsv[2];
				break;
			case 4:
				rgb[0] = t;
				rgb[1] = p;
				rgb[2] = hsv[2];
				break;
			case 5:
			default:
				rgb[0] = hsv[2];
				rgb[1] = p;
				rgb[2] = q;
				break;
			}
			return rgb;     
		}
	}
}

struct Obb {
	typedef Eigen::Affine3f Transformation;
	// origin box to world (without scaling)
	Transformation transformation;
	Eigen::Vector3f size;

	Obb() {}
	Obb( const Transformation &transformation, const Eigen::Vector3f &size ) :
		transformation( transformation ),
		size( size ) {
	}

	Eigen::AlignedBox3f asLocalAlignedBox3f() const {
		return Eigen::AlignedBox3f( -size * 0.5f, size * 0.5f );
	}
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

inline Obb makeOBB( const Eigen::Affine3f &transformation, const Eigen::AlignedBox3f &alignedBox );

inline Eigen::AlignedBox3f Eigen_getTransformedAlignedBox( const Eigen::Projective3f &transformation, const Eigen::AlignedBox3f &source ) {
	Eigen::AlignedBox3f transformed;
	for( int cornerIndex = 0 ; cornerIndex < 8 ; cornerIndex++ ) {
		const Eigen::Vector3f corner = source.corner( Eigen::AlignedBox3f::CornerType( cornerIndex ) );
		// fuck you Eigen! [10/17/2012 kirschan2]
		const Eigen::Vector3f transformedCorner = (transformation * Eigen::Vector4f( corner[0], corner[1], corner[2], 1.0 )).matrix().hnormalized();
		transformed.extend( transformedCorner );
	}
	return transformed;
}

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
bool intersectRayWithUnitCube( const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f *hitPoint = nullptr, float *hitT = nullptr );
bool intersectRayWithAABB( const Eigen::AlignedBox3f &box, const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f *hitPoint = nullptr, float *hitT = nullptr );
bool intersectRayWithOBB( const Obb &obb, const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f *hitPoint = nullptr, float *hitT = nullptr );

inline const Eigen::Vector3f flipSign( const Eigen::Vector3f &v, const Eigen::Vector3f &c ) {
	return Eigen::Vector3f(
		(c[0] > 0.0) ? v[0] : -v[0],
		(c[1] > 0.0) ? v[1] : -v[1],
		(c[2] > 0.0) ? v[2] : -v[2]
	);
}

inline bool intersectRayWithUnitCube( const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f *hitPoint, float *hitT ) {
	using namespace Eigen;

	// we use the symmetry around the origin
	// put position in the first octant
	const Vector3f symPosition = flipSign( position, position );
	const Vector3f symDirection = flipSign( direction, position );

	const float epsilon = 0.000005f;
	const float OneEpsilon = 1.000005f;

	if( symPosition.maxCoeff() < OneEpsilon ) {
		if( hitPoint )
			*hitPoint = position;
		if( hitT )
			*hitT = 0.0f;
		return true;
	}

	// only need to check 3 possible intersection planes (those with positive normal)
	float t[3];
	for( int i = 0 ; i < 3 ; i++ ) {
		if( symDirection[i] != 0.0 ) {
			t[i] = float( (1.0 - symPosition[i]) / symDirection[i] );
		}
		else {
			t[i] = -1.0f;
		}
	}

	if( t[0] >= 0.0f ) {
		auto point = position + direction * t[0];

		if( fabs( point[1] ) < OneEpsilon && fabs( point[2] ) < OneEpsilon ) {
			if( hitPoint ) {
				*hitPoint = point;
			}
			if( hitT ) {
				*hitT = t[0];
			}
			return true;
		}
	}

	if( t[1] >= 0.0f ) {
		auto point = position + direction * t[1];

		if( fabs( point[0] ) < OneEpsilon && fabs( point[2] ) < OneEpsilon ) {
			if( hitPoint ) {
				*hitPoint = point;
			}
			if( hitT ) {
				*hitT = t[1];
			}
			return true;
		}
	}

	if( t[2] >= 0.0f ) {
		auto point = position + direction * t[2];

		if( fabs( point[0] ) < OneEpsilon && fabs( point[1] ) < OneEpsilon) {
			if( hitPoint ) {
				*hitPoint = point;
			}
			if( hitT ) {
				*hitT = t[2];
			}
			return true;
		}
	}

	return false;
}

inline bool intersectRayWithAABB( const Eigen::AlignedBox3f &box, const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f *hitPoint, float *hitT ) {
	using namespace Eigen;

	// box to world
	const auto transformation = Translation3f( box.center() ) * Scaling( box.sizes() / 2 );
	const auto invTransformation = Scaling( box.sizes() / 2 ).inverse() * Translation3f( -box.center() );
	const Vector3f transformedDirection = invTransformation.linear() * direction;
	const Vector3f transformedPosition = invTransformation * position;

	if( intersectRayWithUnitCube( transformedPosition, transformedDirection, hitPoint, hitT ) ) {
		// transform back
		if( hitPoint ) {
			*hitPoint = transformation * *hitPoint;
		}

		return true;
	}
	return false;
}

inline bool intersectRayWithOBB( const Obb &obb, const Eigen::Vector3f &position, const Eigen::Vector3f &direction, Eigen::Vector3f *hitPoint, float *hitT ) {
	using namespace Eigen;

	// box to world
	const auto transformation = obb.transformation * Scaling( obb.size / 2 );
	const auto invTransformation = Scaling( obb.size / 2 ).inverse() * obb.transformation.inverse();
	const Vector3f transformedDirection = invTransformation.linear() * direction;
	const Vector3f transformedPosition = invTransformation * position;

	if( intersectRayWithUnitCube( transformedPosition, transformedDirection, hitPoint, hitT ) ) {
		// transform back
		if( hitPoint ) {
			*hitPoint = transformation * *hitPoint;
		}

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

inline Obb makeOBB( const Eigen::Affine3f &transformation, const Eigen::AlignedBox3f &alignedBox ) {
	return Obb(
		transformation * Eigen::Translation3f( alignedBox.center() ),
		alignedBox.sizes()
	);
}
