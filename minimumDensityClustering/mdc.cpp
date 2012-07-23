#include <vector>
#include <utility>

struct SufficientDensityClusters {
	// points[clusterId][pointIndex]
	std::vector<std::vector<int>> clusterSeedPoints, clusterPoints;
};

template<typename Point, typename LengthType>
SufficientDensityClusters computeSufficientDensityClusters( const std::vector<Point> &points, LengthType metric, int minNeighbors, float radius/*, float fuzzyness*/ ) {
	const int numPoints = points.size();

	// a seed point is a point with sufficient density
	// seed point list
	std::vector<int> seedPoints;
	// seed point characteristic map
	std::vector<bool> isSeedPoint(numPoints);
	// neighborhood list
	std::vector<std::vector<int>> neighbors(numPoints);

	// calculate distances	
	//const float fuzzyRadius = fuzzyness * radius;

	// FIXME: this can be optimized...
	for( int a = 0 ; a < numPoints ; ++a ) {
		int numNeighbors = 0;
		for( int b = 0 ; b < numPoints ; ++b ) {
			if( a == b ) {
				continue;
			}

			const float distance = metric( points[a], points[b] );
			/*if( distance <= fuzzyRadius ) {
				neighbors[a].push_back( b );
			}*/
			if( distance <= radius ) {
				neighbors[a].push_back( b );
				++numNeighbors;
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

	SufficientDensityClusters result;

	if( seedPoints.empty() ) {
		return result;
	}	

	std::vector<bool> isSeedInCluster(numPoints);

	std::vector<int> clusterSeedPoints, clusterPoints;
	std::vector<bool> isInCurrentCluster(numPoints);

	const int numSeeds = seedPoints.size();
	for( int seedIndex = 0 ; seedIndex < numSeeds ; ++seedIndex ) {		
		const int masterSeedPoint = seedPoints[seedIndex];

		if( isSeedInCluster[ masterSeedPoint ] ) {
			// already assigned to a cluster
			continue;
		}

		isInCurrentCluster.clear();
		clusterSeedPoints.clear();
		clusterSeedPoints.clear();
		isInCurrentCluster.resize( numPoints );

		// add master seed point
		isSeedInCluster[ masterSeedPoint ] = true;
		isInCurrentCluster[ masterSeedPoint ] = true;
		clusterSeedPoints.push_back( masterSeedPoint );
		clusterPoints.push_back( masterSeedPoint );

		for( int index = 0 ; index < clusterSeedPoints.size() ; ++index ) {
			const int seedPoint = clusterSeedPoints[ index ];
			const auto &seedNeighbors = neighbors[seedPoint];

			for( int neighborIndex = 0 ; neighborIndex < seedNeighbors.size() ; ++neighborIndex ) {
				const int neighborPoint = seedNeighbors[ neighborIndex ];

				if( isInCurrentCluster[ neighborPoint ] ) {
					continue;
				}

				if( isSeedPoint[ neighborPoint ] ) {
					isSeedInCluster[ neighborPoint ] = true;
					clusterSeedPoints.push_back( neighborPoint );					
				}
				
				isInCurrentCluster[ neighborPoint ] = true;
				clusterPoints.push_back( neighborPoint );				
			}
		}

		result.clusterSeedPoints.push_back( std::move( clusterSeedPoints ) );
		result.clusterPoints.push_back( std::move( clusterPoints ) );
	}

	return result;
}

#include <cstdlib>

struct TestPoint {
	int x, y;

	TestPoint() {}
	TestPoint( int x, int y ) : x(x), y(y) {}

	static float distance( const TestPoint &a, const TestPoint &b ) {
		int dx = a.x - b.x;
		int dy = a.y - b.y;
		return abs( dx ) + abs( dy );
	}
};

void main() {
	std::vector<TestPoint> points;

	for( int x = 0 ; x < 64 ; x++ ) {
		for( int y = 0 ; y < 64 ; y++ ) {
			points.push_back( TestPoint( x * 4, y * 4 ) );
		}
	}

	for( int x = 0 ; x < 32 ; x++ ) {
		for( int y = 0 ; y < 32 ; y++ ) {
			points.push_back( TestPoint( x * 2, y * 2 ) );
		}
	}

	SufficientDensityClusters clusters = computeSufficientDensityClusters( points, TestPoint::distance, 4, 2.0 );
}