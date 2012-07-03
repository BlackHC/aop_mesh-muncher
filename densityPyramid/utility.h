#pragma once

#include "niven.Core.Core.h"
#include "niven.Core.Math.Vector.h"
#include "niven.Core.Math.VectorFunctions.h"
#include "niven.Core.Math.VectorToString.h"

namespace niven {
	/*Vector3i indexToCubeCorner(int i ) {
		return Vector3i( (i & 1) ? 1 : 0, (i & 2) ? 1 : 0, (i & 4) ? 1 : 0 );
	}*/

	const Vector3i indexToCubeCorner[] = {
		Vector3i( 0,0,0 ), Vector3i( 1,0,0 ), Vector3i( 0,1,0 ), Vector3i( 0,0,1 ),
		Vector3i( 1,1,0 ), Vector3i( 0,1,1 ), Vector3i( 1,0,1 ), Vector3i( 1,1,1 )
	};

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

	inline int squaredMaxDistanceAABoxPoint( const Vector3i &min, const Vector3i &max, const Vector3i &point ) {
		Vector3i distanceA = VectorAbs( point - min );
		Vector3i distanceB = VectorAbs( point - max );
		Vector3i distance = VectorMax( distanceA, distanceB );

		return LengthSquared( distance );
	}
}