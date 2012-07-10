#pragma once

#include "niven.Core.Core.h"
#include "niven.Core.Math.Vector.h"
#include "niven.Core.Math.VectorFunctions.h"
#include "niven.Core.Math.VectorToString.h"
#include <stdlib.h>

namespace niven {
	/*Vector3i indexToCubeCorner(int i ) {
		return Vector3i( (i & 1) ? 1 : 0, (i & 2) ? 1 : 0, (i & 4) ? 1 : 0 );
	}*/

	const Vector3i indexToCubeCorner[] = {
		Vector3i( 0,0,0 ), Vector3i( 1,0,0 ), Vector3i( 0,1,0 ), Vector3i( 0,0,1 ),
		Vector3i( 1,1,0 ), Vector3i( 0,1,1 ), Vector3i( 1,0,1 ), Vector3i( 1,1,1 )
	};

	inline Vector3i nearestPointOnAABoxToPoint( const Vector3i &min, const Vector3i &max, const Vector3i &point ) {
		Vector3i nearestPoint;
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


	inline int squaredDistanceAABoxPoint( const Vector3i &min, const Vector3i &max, const Vector3i &point ) {
		Vector3i distance;
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
		return LengthSquared( distance );
	}

	inline Vector3i farthestPointOnAABoxToPoint( const Vector3i &min, const Vector3i &max, const Vector3i &point ) {
		Vector3i farthestPoint;
		for( int i = 0 ; i < 3 ; i++ ) {
			if( abs( point[i] - min[i] ) > abs( point[i] - max[i] ) ) {
				farthestPoint[i] = min[i];
			}
			else {
				farthestPoint[i] = max[i];
			}
		}
		return farthestPoint;
	}

	inline int squaredMaxDistanceAABoxPoint( const Vector3i &min, const Vector3i &max, const Vector3i &point ) {
		Vector3i distanceA = VectorAbs( point - min );
		Vector3i distanceB = VectorAbs( point - max );
		Vector3i distance = VectorMax( distanceA, distanceB );

		return LengthSquared( distance );
	}

	inline int squaredMinDistanceAABAAB( const Vector3i &minA, const Vector3i &maxA, const Vector3i &minB, const Vector3i &maxB ) {
		Vector3i distance;

		for( int i = 0 ; i < 3 ; i++ ) {
			// overlap?
			if( minB[i] <= maxA[i] && minA[i] <= maxB[i] ) {
				distance[i] = 0;
			}
			else { // => minB[i] > maxA[i] || minA[i] > maxB[i]
				// no overlap
				if( minB[i] > maxA[i] ) {
					distance[i] = minB[i] - maxA[i];
				}
				else { // => minA[i] > maxB[i]
					distance[i] = minA[i] - maxB[i];
				}
			}
		}
		return LengthSquared( distance );
	}

	template<typename Type, int Size>
	Vector<Type, Size> VecCompMult( const Vector<Type, Size> &a, const Vector<Type, Size> &b ) {
		Vector<Type, Size> result;
		for( int i = 0 ; i < Size ; i++ ) {
			result[i] = a[i] * b[i];
		}
		return result;
	}
}