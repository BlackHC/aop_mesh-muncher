/*
sample twice from box and sphere and compare the (transformed) distance fields

discriminate between a box and a sphere
*/

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

#include "Eigen/Eigen"

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
		const Vector3i minCorner = _min - center;
		const Vector3i maxCorner = _max - center;

		const int z = positiveAxis ? minCorner( axisSwizzle[2] ) : -maxCorner( axisSwizzle[2] );
		if( z <= 0 ) {
			// try the far plane
			z = positiveAxis ? maxCorner( axisSwizzle[2] ) : -minCorner( axisSwizzle[2] );
			if( z <= 0 ) {
				// the box is on the other side of the origin
				return CVT_NO_MATCH;
			}

			for( int i = 0 ; i < 2 ; i++  ) {
				if( maxCorner( axisSwizzle[ i ] ) < -z || z < minCorner( axisSwizzle[ i ] ) ) {
					return CVT_NO_MATCH;
				}
			}
			// its a partial match at most
			return CVT_PARTIAL;
		}

		for( int i = 0 ; i < 2 ; i++  ) {
			if( maxCorner( axisSwizzle[ i ] ) < -z || z < minCorner( axisSwizzle[ i ] ) ) {
				return CVT_NO_MATCH;
			}
		}
		// => -z <= maxCorner( axisSwizzle( i ) ) && minCorner( axisSwizzle( i ) ) <= z
		for( int i = 0 ; i < 2 ; i++  ) {
			if( minCorner( axisSwizzle[ i ] ) < -z || z < maxCorner( axisSwizzle[ i ] ) ) {
				return CVT_PARTIAL;
			}
		}
		// => maxCorner( axisSwizzle( i ) ) <= z && -z <= minCorner( axisSwizzle( i ) ) && -z <= maxCorner( axisSwizzle( i ) ) && minCorner( axisSwizzle( i ) ) <= z
		return CVT_MATCH;
	}
};

template< int zAxis, bool positiveAxis >
const int AxisCone<zAxis, positiveAxis>::axisSwizzle[] = { (zAxis + 1) % 3, (zAxis + 2) % 3, zAxis };

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

const float MAX_DISTANCE = 128.0f;

struct UnorderedDistanceContext {
	static const int numSamples = 27;
	static Vector3f directions[numSamples];
	float distances[numSamples];

	static void setDirections() {
		for( int i = 0 ; i < numSamples ; i++ ) {
			directions[i] = neighborOffsets[i].Cast<float>();
		}
	}

	void fill( DenseCache &cache, const Vector3i &position ) {
		for( int index = 0 ; index < numSamples ; ++index ) {
				RayQuery query( position.Cast<float>(), directions[index] );

				distances[index] = std::min( MAX_DISTANCE, Math::Sqrt<float>( findSquaredDistanceToNearestConditionedVoxel( cache, position, query ) ) );
		}
		std::sort( distances, distances + numSamples );
	}

	double calculateAverage() const {
		double average = 0.;
		for( int index = 0 ; index < numSamples ; ++index ) {
			average += distances[index];
		}
		average /= numSamples;
		return average;
	}

	void normalizeWithAverage() {
		double average = calculateAverage();
		for( int index = 0 ; index < numSamples ; ++index ) {
			distances[index] /= average;
		}
	}

	static double compare( const UnorderedDistanceContext &a, const UnorderedDistanceContext &b ) {
		// use L2 norm because it accentuates big differences better than L1
		double norm = 0.;
		for( int index = 0 ; index < numSamples ; ++index ) {
			double delta = a.distances[index] - b.distances[index];
			norm = std::max( norm, Math::Abs(delta) );
		}
		return norm;
	}
};

Vector3f UnorderedDistanceContext::directions[UnorderedDistanceContext::numSamples];

typedef UnorderedDistanceContext Probe;
typedef std::vector<Probe> ProbeVector;

template< typename Data >
struct DataVolume {
	Vector3i min, size;
	int step;
	Vector3i probeDims;
	int probeCount;

	MemoryLayout3D layout;

	Data *data;

	DataVolume( Vector3i min, Vector3i size, int step ) : min( min ), size( size ), step( step ), probeDims( (size + Vector3i::Constant(step-1)) / step ), probeCount( probeDims.X() * probeDims.Y() * probeDims.Z() ), layout( probeDims ) {
		data = new Data[probeCount];
	}

	~DataVolume() {
		delete[] data;
	}
	
	Iterator3D getIterator() const { return Iterator3D(layout); }

	Vector3i getPosition( const Iterator3D &it ) const {
		return VecCompMult( it.ToVector(), probeDims ) + min;
	}

	Data& operator[] (const Iterator3D &it) {
		return data[ layout( it.ToVector() ) ];
	}

	const Data& operator[] (const Iterator3D &it) const {
		return data[ layout( it.ToVector() ) ];
	}

	bool validIndex( const Vector3i &index ) const {
		for( int i = 0 ; i < 3 ; ++i ) {
			if( index[i] < 0 || index[i] >= probeDims[i] ) {
				return false;
			}
		}
		return true;
	}
};

typedef DataVolume<Probe> Probes;

const float PROBE_MAX_DELTA = 16;

Vector3i getCubeSize( const Vector3i &min, const Vector3i &max ) {
	return max - min + Vector3i::Splat(1);
}

int getVolume( const Vector3i &size ) {
	return size.X() * size.Y() * size.Z();
}

bool canMatchProbe( const ProbeVector &refProbes, const Probe &probe ) {
	for( int i = 0 ; i < refProbes.size() ; ++i ) {
		if( Probe::compare( refProbes[i], probe ) < PROBE_MAX_DELTA ) {
			return true;
		}
	}
	return false;
}

double calcMatchRatio( const DataVolume<bool> &matches, const Vector3i &min, const Vector3i &max ) {
	const Vector3i size = getCubeSize(min, max);
	int count = 0;
	for( Iterator3D it( min, size ) ; !it.IsAtEnd() ; ++it ) {
		if( !matches.validIndex( it.ToVector() ) ) {
			continue;
		}

		if( matches[it] ) {
			++count;
		}
	}
	return double( count ) / getVolume( size );
}

// in positions = data volume indices
void getExpandedCubeSide( int side, const Vector3i &min, const Vector3i &max, Vector3i &outMin, Vector3i &outMax ) {
	outMin = min;
	outMax = max;
	switch( side ) {
	case 0:
		outMin.X() = outMax.X() += 1;
		break;
	case 1:
		outMin.Y() = outMax.Y() += 1;
		break;
	case 2:
		outMin.Z() = outMax.Z() += 1;
		break;
	case 3:
		outMax.X() = outMin.X() -= 1;
		break;
	case 4:
		outMax.Y() = outMin.Y() -= 1;
		break;
	case 5:
		outMax.Z() = outMin.Z() -= 1;
		break;
	}
}

void expandVolume( const DataVolume<bool> &matches, const Vector3i &startPosition, int targetVolume, Vector3i &min, Vector3i &max ) {
	struct Candidate {
		Vector3i min, max;
		double matchRatio;
	};

	Candidate currentSolution;
	currentSolution.min = currentSolution.max = startPosition;
	currentSolution.matchRatio = 1.0;
	while(getVolume(getCubeSize(currentSolution.min,currentSolution.max)) < targetVolume ) {
		Candidate candidates[6];

		for( int i = 0 ; i < 6 ; i++ ) {
			Candidate &candidate = candidates[i];
			getExpandedCubeSide( i, currentSolution.min, currentSolution.max, candidate.min, candidate.max);
			candidate.matchRatio = calcMatchRatio( matches, candidate.min, candidate.max );
		}

		double bestRatio = candidates[0].matchRatio;
		Vector3i bestMin, bestMax;
		bestMin = VectorMin( currentSolution.min, candidates[0].min );
		bestMax = VectorMax( currentSolution.max, candidates[0].max );
		for( int i = 1 ; i < 6 ; i++ ) {
			if( candidates[i].matchRatio > bestRatio ) {
				bestRatio = candidates[i].matchRatio;
				bestMin = VectorMin( currentSolution.min, candidates[i].min );
				bestMax = VectorMax( currentSolution.max, candidates[i].max );
			}
		}
		currentSolution.min = bestMin;
		currentSolution.max = bestMax;
		currentSolution.matchRatio = bestRatio;
	}

	min = currentSolution.min;
	max = currentSolution.max;
}

Vector3f getMean( const DataVolume<bool> &matches, const Vector3i &min, const Vector3i &max ) {
	int count;
	Vector3f mean = Vector3f::CreateZero();
	for( Iterator3D it(min, getCubeSize( min, max )) ; !it.IsAtEnd() ; ++it ) {
		if( matches[ it ] ) {
			++count;
			mean += it.ToVector().Cast<float>();
		}
	}
	return mean / count;
}

void PCA( const DataVolume<bool> &matches, const Vector3i &min, const Vector3i &max ) {
	Vector3f mean = getMean( matches, min, max );
	Eigen::Matrix3f covarianceMatrix = Eigen::Matrix3f::Zero();

	int count = 0;
	for( Iterator3D it(min, getCubeSize( min, max )) ; !it.IsAtEnd() ; ++it ) {
		Vector3f shifted = it.ToVector().Cast<float>() - mean;
		float xx = shifted.X() * shifted.X();
		float yy = shifted.Y() * shifted.Y();
		float zz = shifted.Z() * shifted.Z();

		float xy = shifted.X() * shifted.Y();
		float xz = shifted.X() * shifted.Z();
		float yz = shifted.Y() * shifted.Z();

		covarianceMatrix(0,0) += xx;
		covarianceMatrix(1,1) += yy;
		covarianceMatrix(2,2) += zz;
		covarianceMatrix(1,0) += xy;
		covarianceMatrix(2,0) += xz;
		covarianceMatrix(2,1) += yz;

		count++;
	}
	covarianceMatrix(0,1) = covarianceMatrix(1,0);
	covarianceMatrix(0,2) = covarianceMatrix(2,0);
	covarianceMatrix(1,2) = covarianceMatrix(2,1);

	covarianceMatrix /= count;

	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigenSolver(covarianceMatrix);
	std::cout << "The eigenvalues of A are:\n" << eigenSolver.eigenvalues() << std::endl;
	std::cout << "Here's a matrix whose columns are eigenvectors of A \n"
		<< "corresponding to these eigenvalues:\n"
		<< eigenSolver.eigenvectors() << std::endl;
}

struct MinimumDensityClusterResult {
	//int numPoints;
	int numClusters; 
	// 0 for no cluster
	std::vector<int> cluster;
};

template<typename Point, typename LengthType>
void computeMinimumDensityClusters( const std::vector<Point> &points, LengthType metric, int minNeighbors, float radius/*, float fuzzyness*/ ) {
	std::vector<int> seedPoints;
	std::vector<bool> isSeedPoint(points.size());
	std::vector<std::vector<int>> neighbors(points.size());

	int numClusters;
	std::vector<int> clusters;

	// calculate distances
	const int numPoints = points.size();

	//const float fuzzyRadius = fuzzyness * radius;

	for( int a = 0 ; a < numPoints ; ++a ) {
		int numNeighbors = 0;
		for( int b = 0 ; b < numPoints ; ++b ) {
			const float distance = metric( points[a], points[b] );
			/*if( distance <= fuzzyRadius ) {
				neighbors[a].push_back( b );
			}*/
			if( distance <= radius ) {
				neighbors[a].push_back( b );
				++numNeigbors;
			}
		}
		if( numNeighbors >= minNeighbors ) {
			seedPoints.push_back(a);
			isSeedPoint[a] = true;
		}
		else {
			isSeedPoint[a] = false;
		}
	}

	if( seedPoints.empty() ) {
		return;
	}

	const int numSeeds = seedPoints.size();
	for( int seed = 0 ; seed < numSeeds ; ++seed ) {
		if( clusters[seed] != 0 ) {
			// already in cluster
			continue;
		}
		++numClusters;

		std::vector<int> stack;
		stack.push_back( seed );
		while( !stack.empty() ) {
			int point = stack.back();
			stack.pop_back();

			clusters[point] = numClusters;

			if( !isSeedPoint[point] ) {
				continue;
			}

			stack.insert( stack.end(), neighbors[point].cbegin(), neighbors[point].cend() );
		}
	}
}

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

		UnorderedDistanceContext::setDirections();

		const Vector3i min(850,120,120);
		const Vector3i size(280, 280, 280);
		Probes probes(min, size, 16);

		for( Iterator3D it = probes.getIterator() ; !it.IsAtEnd() ; ++it ) {
			Probe &probe = probes[it];
			probe.fill( cache, probes.getPosition( it ) );
			//probes[ it ].normalizeWithAverage();

			/*std::cout << StringConverter::ToString(it.ToVector()) << " " << StringConverter::ToString( probes.getPosition( it ) );
			for( int i = 0 ; i < Probe::numSamples ; i++ ) {
				std::cout << " " << probe.distances[i];
			}
			std::cout << "\n";*/
		}

		//std::cout << "\n";

		ProbeVector refProbes;
		for( Iterator3D it( Vector3i(4,1,1 ) ) ; !it.IsAtEnd() ; ++it ) {
			refProbes.push_back( probes[it] );
		}

		DataVolume<bool> matches( min, size, 16 );
		for( Iterator3D it = probes.getIterator() ; !it.IsAtEnd() ; ++it ) {
			matches[ it ] = canMatchProbe( refProbes, probes[ it ] );
		}

		/*Vector3i sugMin, sugMax;
		expandVolume( matches, Vector3i( 0,0,0 ), 4, sugMin, sugMax );*/
		/*
		for( Iterator3D it = matches.getIterator() ; !it.IsAtEnd() ; ++it ) {
			std::cout << StringConverter::ToString(it.ToVector()) << ": " << matches[ it ] << "\n";
		}*/

		PCA( matches, Vector3i(0,0,0), Vector3i(3,0,0) );
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