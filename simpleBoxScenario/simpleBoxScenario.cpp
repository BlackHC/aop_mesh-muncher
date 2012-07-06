#include "Core/inc/Core.h"
#include "Core/inc/Exception.h"
#include "Core/inc/Log.h"
#include "Core/inc/nString.h"

#include "niven.Core.IO.Path.h"

#include "niven.Volume.FileBlockStorage.h"
#include "niven.Volume.MarchingCubes.h"
#include "niven.Volume.Volume.h"

#include "niven.Core.Geometry.Plane.h"

#include "niven.Core.MemoryLayout.h"
#include "niven.Core.Iterator3D.h"

#include "niven.Core.Math.VectorToString.h"

#include "findDistances.h"

#include "niven.Engine.Sample.h"
#include "niven.Core.Math.3dUtility.h"
#include "niven.Engine.Spatial.AxisAlignedBoundingBox.h"
#include "niven.Core.Geometry.Ray.h"

#include <iostream>

using namespace niven;

/*
TODO: use different layer for mipmaps!!
*/

const Vector3i neighborOffsets[] = {
	// z = 0
	Vector3i( -1, 0, 0  ), Vector3i( 1, 0, 0 ), 
	Vector3i( -1, -1, 0 ), Vector3i( 0, -1, 0 ), Vector3i( 1, -1, 0 ),
	Vector3i( -1, 1, 0 ), Vector3i( 0, 1, 0 ), Vector3i( 1, 1, 0 ),
	// z = -1
	Vector3i( -1, 0, -1 ), Vector3i( 0, 0, -1 ), Vector3i( 1, 0, -1 ), 
	Vector3i( -1, -1, -1 ), Vector3i( 0, -1, -1 ), Vector3i( 1, -1, -1 ),
	Vector3i( -1, 1, -1 ), Vector3i( 0, 1, -1 ), Vector3i( 1, 1, -1 ),
	// z = 1
	Vector3i( -1, 0, 1 ), Vector3i( 0, 0, 1 ), Vector3i( 1, 0, 1 ), 
	Vector3i( -1, -1, 1 ), Vector3i( 0, -1, 1 ), Vector3i( 1, -1, 1 ),
	Vector3i( -1, 1, 1 ), Vector3i( 0, 1, 1 ), Vector3i( 1, 1, 1 ),
};

template< int zAxis, bool positiveAxis >
struct AxisCone {
	// after swizzling: assume cone along z
	const static int axisSwizzle[];

	static ConditionedVoxelType query(const Vector3i &_min, const Vector3i &_max, const Vector3i &center ) {
		const Vector3i min = _min - center;
		const Vector3i max = _max - center;

		const int z = positiveAxis ? min( axisSwizzle[2] ) : -max( axisSwizzle[2] );
		if( z <= 0 ) {
			// try the far plane
			z = positiveAxis ? max( axisSwizzle[2] ) : -min( axisSwizzle[2] );
			if( z <= 0 ) {
				return CVT_NO_MATCH;
			}

			for( int i = 0 ; i < 2 ; i++  ) {
				if( max( axisSwizzle[ i ] ) < -z || z < min( axisSwizzle[ i ] ) ) {
					return CVT_NO_MATCH;
				}
			}
			// its a partial match at most
			return CVT_PARTIAL;
		}

		for( int i = 0 ; i < 2 ; i++  ) {
			if( max( axisSwizzle[ i ] ) < -z || z < min( axisSwizzle[ i ] ) ) {
				return CVT_NO_MATCH;
			}
		}
		// => -z <= max( axisSwizzle( i ) ) && min( axisSwizzle( i ) ) <= z
		for( int i = 0 ; i < 2 ; i++  ) {
			if( min( axisSwizzle[ i ] ) < -z || z < max( axisSwizzle[ i ] ) ) {
				return CVT_PARTIAL;
			}
		}
		// => max( axisSwizzle( i ) ) <= z && -z <= min( axisSwizzle( i ) )     && -z <= max( axisSwizzle( i ) ) && min( axisSwizzle( i ) ) <= z
		return CVT_MATCH;
	}
};

template< int zAxis, bool positiveAxis >
const int AxisCone<zAxis, positiveAxis>::axisSwizzle[] = { (zAxis + 1) % 3, (zAxis + 2) % 3, zAxis };

template<int axis, bool positiveAxis>
struct AARay {
	const static int axisSwizzle[];

	static ConditionedVoxelType query(const Vector3i &_min, const Vector3i &_max, const Vector3i &center ) {
		const Vector3i min = _min - center;
		const Vector3i max = _max - center;

		const int z = positiveAxis ? max[ axisSwizzle[2] ] : -min[ axisSwizzle[2] ];
		if( z < 0 ) {
			return CVT_NO_MATCH;
		}

		for( int i = 0 ; i < 2 ; i++ ) {
			if( max[ axisSwizzle[i] ] < 0 || 0 < min[ axisSwizzle[i] ] ) {
				return CVT_NO_MATCH;
			}
		}
		// => 0 <= max( axisSwizzle( i ) ) && min( axisSwizzle( i ) ) <= 0
		for( int i = 0 ; i < 2 ; i++  ) {
			if( min[ axisSwizzle[i] ] < 0 || 1 < max[ axisSwizzle[i] ] ) {
				return CVT_PARTIAL;
			}
		}
		return CVT_MATCH;
	}
};

template< int zAxis, bool positiveAxis >
const int AARay<zAxis, positiveAxis>::axisSwizzle[] = { (zAxis + 1) % 3, (zAxis + 2) % 3, zAxis };
/*
template<typename PolytopeA, typename PolytopeB>
struct ConvexPolytopeCollision {
	ConditionedVoxelType result;
	PolytopeA A;
	PolytopeB B;

	template<typename PolytopeX, typename PolytopeY>
	static bool overlapPointsOfXAndPlanesOfY( const PolytopeX &X, const PolytopeY &Y, ConditionedVoxelType &result) {
		bool inside = true;
		for( int j = 0 ; j < PolytopeY::numPlanes ; ++j ) {
			bool outside = true;
			const Plane<float> &plane = Y.getPlane(j);

			for( int i = 0 ; i < PolytopeX::numPoints ; ++i ) {
				if( plane.Distance( X.getPoint(i) ) > 0 ) {
					inside = false;
				}
				else {
					outside = false;
				}
			}
			for( int i = 0 ; i < PolytopeX::numDirections ; ++i ) {
				if( Dot( plane.GetNormal(), X.getDirection(i) ) > 0 ) {
					inside = false;
				}
				else {
					outside = false;
				}
			}
			if( outside ) {
				result = CVT_NO_MATCH;
				return true;
			}
		}
		if( inside ) {
			result = CVT_MATCH;
			return true;
		}
		return false;
	}

	template<typename PolytopeX, typename PolytopeY>
	static ConditionedVoxelType polytopeOverlap( const PolytopeX &X, const PolytopeY &Y ) {
		ConditionedVoxelType result;
		if( overlapPointsOfXAndPlanesOfY( X, Y, result ) ) {
			return result;
		}
		if( overlapPointsOfXAndPlanesOfY( Y, X, result ) ) {
			return result;
		}
		return CVT_PARTIAL;
	}
};*/

struct RayQuery {
	const Ray<Vector3f> ray;

	RayQuery( const Vector3f &start, const Vector3f &direction ) : ray( start, direction ) {}

	ConditionedVoxelType operator() (const Vector3i &_min, const Vector3i &_max) {
		return AxisAlignedBoundingBox3( _min.Cast<float>(), _max.Cast<float>() ).Trace( ray ) ? CVT_PARTIAL : CVT_NO_MATCH;
	}
};

struct FrutumQuery {
	// normals pointing outward
	Plane<float> planes[4];
	
	FrutumQuery( const Vector3f &center, const Vector3f &p1, const Vector3f &p2, const Vector3f &p3, const Vector3f &p4 ) {
		planes[0] = Plane<float>::CreateFromPoints( p1, p2, center );
		planes[1] = Plane<float>::CreateFromPoints( p4, p1, center );
		planes[2] = Plane<float>::CreateFromPoints( p3, p4, center );
		planes[3] = Plane<float>::CreateFromPoints( p2, p3, center );
	}

	FrutumQuery( const Vector3f &center, const Vector3f &direction, const float tanHalfFOV ) {
		Vector3f u, v;
		CreateCoordinateSystem( direction, u, v );

		planes[0] = Plane<float>::CreateFromNormalPoint( -u - tanHalfFOV * direction, center );
		planes[1] = Plane<float>::CreateFromNormalPoint( -v - tanHalfFOV * direction, center );
		planes[2] = Plane<float>::CreateFromNormalPoint( u - tanHalfFOV * direction, center );
		planes[3] = Plane<float>::CreateFromNormalPoint( v - tanHalfFOV * direction, center );
	}

	ConditionedVoxelType operator() ( const Vector3i &_min, const Vector3i &_max ) {
		const Vector3f min = _min.Cast<float>();
		const Vector3f size = (_max - _min).Cast<float>();
		
		bool inside = true;
		for( int p = 0 ; p < 4 ; ++p ) {
			bool outside = true;
			for( int i = 0 ; i < 8 ; ++i ) {
				const Vector3f corner = min + size.X() * indexToCubeCorner[i].Cast<float>();

				if( planes[p].Distance( corner ) >= 0 ) {
					inside = false;
				}
				else {
					outside = false;
				}
			}
			if( outside ) {
				return CVT_NO_MATCH;
			}
		}
		if( inside ) {
			return CVT_MATCH;
		}
		// conservative solution
		return CVT_PARTIAL;
	}
};
/*
struct ConeQuery {
	Vector3f start;
	Vector3f position;
	float radius;

	ConditionedVoxelType query( const Vector3i &_min, const Vector3i &_max ) {

};*/

void findEmptySpaceInBox( DenseCache &cache, const Vector3i &position, /*const Vector3i &minSize,*/ Vector3i &minEmpty, Vector3i &maxEmpty ) {
	int posX = floor( sqrt( (float) findSquaredDistanceToNearestConditionedVoxel( cache, position, [&] (const Vector3i &min, const Vector3i &max ) { return AARay<0,true>::query( min, max, position ); } ) ));
	int posY = floor( sqrt( (float) findSquaredDistanceToNearestConditionedVoxel( cache, position, [&] (const Vector3i &min, const Vector3i &max ) { return AARay<1,true>::query( min, max, position ); } ) ));
	int posZ = floor( sqrt( (float) findSquaredDistanceToNearestConditionedVoxel( cache, position, [&] (const Vector3i &min, const Vector3i &max ) { return AARay<2,true>::query( min, max, position ); } ) ));

	int negX = floor( sqrt( (float) findSquaredDistanceToNearestConditionedVoxel( cache, position, [&] (const Vector3i &min, const Vector3i &max ) { return AARay<0,false>::query( min, max, position ); } ) ));
	int negY = floor( sqrt( (float) findSquaredDistanceToNearestConditionedVoxel( cache, position, [&] (const Vector3i &min, const Vector3i &max ) { return AARay<1,false>::query( min, max, position ); } ) ));
	int negZ = floor( sqrt( (float) findSquaredDistanceToNearestConditionedVoxel( cache, position, [&] (const Vector3i &min, const Vector3i &max ) { return AARay<2,false>::query( min, max, position ); } ) ));

	minEmpty = position - Vector3i( negX, negY, negZ );
	maxEmpty = position + Vector3i( posX, posY, posZ );
}

struct DistanceContext {
	static const int numUSteps = 24, numVSteps = 96;
	static Vector3f directions[numUSteps][numVSteps];
	float distances[numUSteps][numVSteps];

	static void setDirections() {
		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			const float u = float(uIndex + 0.5) / numUSteps;
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				const float v = float(vIndex) / numVSteps;
				directions[uIndex][vIndex] = UniformSampleSphere( u, v );
			}
		}
	}

	void fill( DenseCache &cache, const Vector3i &position ) {
		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				//FrutumQuery query( position.Cast<float>(), directions[uIndex][vIndex], Math::Tan( 1.0 ) );
				RayQuery query( position.Cast<float>(), directions[uIndex][vIndex] );

				//distances[uIndex][vIndex] = Math::Sqrt<float>( findSquaredDistanceToNearestConditionedVoxel( cache, position, [&] (const Vector3i &min, const Vector3i &max) { return query.query(min, max); } ) );
				distances[uIndex][vIndex] = Math::Sqrt<float>( findSquaredDistanceToNearestConditionedVoxel( cache, position, query ) );
			}
		}
	}

	double calculateAverage() const {
		double average = 0.;
		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				average += distances[uIndex][vIndex];
			}
		}
		average /= numUSteps * numVSteps;
		return average;
	}

	void normalizeWithAverage() {
		double average = calculateAverage();
		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				distances[uIndex][vIndex] /= average;
			}
		}
	}

	static void getBestDirection( const Vector3f mappedPoints[numUSteps][numVSteps], const Vector3f &direction, int *u, int *v ) {
		float best = -2;
		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				float dot = Dot( direction, Normalize( mappedPoints[uIndex][vIndex] ) );
				if( dot > best ) {
					*u = uIndex;
					*v = vIndex;
					best = dot;
				}
			}
		}
	}

	// normalize into the [-1,1]^3 unit cube
	// the distance field is transformed as if we had sampled from the center of "mass" of the distance field
	void normalizeIntoUnitCube() {
		Vector3f min, max;
		min = max = distances[0][0] * directions[0][0];
		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				Vector3f point = distances[uIndex][vIndex] * directions[uIndex][vIndex];
				min = VectorMin( min, point );
				max = VectorMax( max, point );
			}
		}

		Vector3f size = max - min;
		
		Vector3f mappedPoints[numUSteps][numVSteps];
		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				Vector3f &mappedPoint = mappedPoints[uIndex][vIndex];
				mappedPoint = (distances[uIndex][vIndex] * directions[uIndex][vIndex] - min) * 2;
				for( int i = 0 ; i < 3 ; i++ ) {
					mappedPoint[i] /= size[i];
				}
				mappedPoint += Vector3f::Constant( -1 );
			}
		}

		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				int u, v;
				getBestDirection( mappedPoints, directions[uIndex][vIndex], &u, &v );
				distances[uIndex][vIndex] = Length( mappedPoints[u][v] );
			}
		}
	}

	static double compare( const DistanceContext &a, const DistanceContext &b, int uShift = 0, int vShift = 0 ) {
		// use L2 norm because it accentuates big differences better than L1
		double norm = 0.;
		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				double delta = a.distances[uIndex][vIndex] - b.distances[ (uIndex + uShift) % numUSteps ][ (vIndex + vShift) % numVSteps ];
				norm += delta * delta;
			}
		}
		return Math::Sqrt( norm );
	}

	static double compareAndShift( const DistanceContext &a, const DistanceContext &b, int *uShift = nullptr, int *vShift = nullptr) {
		double bestNorm = DBL_MAX;

		for( int uIndex = 0 ; uIndex < numUSteps ; ++uIndex ) {
			for( int vIndex = 0 ; vIndex < numVSteps ; ++vIndex ) {
				double norm = compare( a, b, uIndex, vIndex );
				if( norm < bestNorm ) {
					bestNorm = norm;

					if( uShift )
						*uShift = uIndex;
					if( vShift )
						*vShift = vIndex;
				}
			}
		}
		return bestNorm;
	}
};

Vector3f DistanceContext::directions[DistanceContext::numUSteps][DistanceContext::numVSteps];


int main(int argc, char* argv[]) 
{
	CoreLifeTimeHelper clth;

	try {
		Volume::FileBlockStorage shardFile; 
		if( !shardFile.Open( "P:\\BlenderScenes\\two_boxes_4.nvf", true ) ) {
			Log::Error( "shard2", "couldn't open the volume file!" );
		}

		MipVolume volume( shardFile );
		DenseCache cache( volume );

		const int centerShift = 32;
		Vector3i centerBox(986,256,256), centerBox2(986 + centerShift,256,256 + centerShift);
		Vector3i centerSphere(256,256,256), centerSphere2(256 + centerShift, 256, 256 + centerShift);

		DistanceContext::setDirections();

		//std::cout << sqrt( (float) findSquaredDistanceToNearestVoxel( cache, center, 0 ) );
		//Vector3i minSpace, maxSpace;
		//findEmptySpaceInBox( cache, center, minSpace, maxSpace );
		//std::cout << StringConverter::ToString(minSpace) << StringConverter::ToString(maxSpace) << "\n";

		DistanceContext contextBox[2], contextSphere[2];
		contextBox[0].fill( cache, centerBox );
		contextBox[0].normalizeIntoUnitCube();
		contextBox[1].fill( cache, centerBox2 );
		contextBox[1].normalizeIntoUnitCube();

		contextSphere[0].fill( cache, centerSphere );
		contextSphere[0].normalizeIntoUnitCube();
		contextSphere[1].fill( cache, centerSphere2 );
		contextSphere[1].normalizeIntoUnitCube();
		
		int uShift, vShift;
		double normB0S0 = DistanceContext::compareAndShift( contextBox[0], contextSphere[0], &uShift, &vShift );
		double normB0S1 = DistanceContext::compareAndShift( contextBox[0], contextSphere[1], &uShift, &vShift );
		double normB1S0 = DistanceContext::compareAndShift( contextBox[1], contextSphere[0], &uShift, &vShift );
		double normB1S1 = DistanceContext::compareAndShift( contextBox[1], contextSphere[1], &uShift, &vShift );
		double normB0B1 = DistanceContext::compareAndShift( contextBox[0], contextBox[1], &uShift, &vShift );
		double normS0S1 = DistanceContext::compareAndShift( contextSphere[0], contextSphere[1], &uShift, &vShift );

		/*std::cout << findDistanceToNearestConditionedVoxel( cache, center, [yMin,yMax](const Vector3i &min, const Vector3i &max) -> ConditionedVoxelType {
			if( max.Z() < yMin || yMax < min.Z() ) {
				return CVT_NO_MATCH;
			}
			if( yMin <= min.Z() && max.Z() <= yMax ) {
				return CVT_MATCH;
			}
			return CVT_PARTIAL;
		} );*/

		shardFile.Flush();

	} catch (Exception& e) {
		std::cerr << e.what() << std::endl;
		std::cerr << e.GetDetailMessage() << std::endl;
		std::cerr << e.where() << std::endl;
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}