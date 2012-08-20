#pragma once

#include <vector>
#include <algorithm>
#include <Eigen/Eigen>
#include <numeric>

struct Point {
	Eigen::Vector3f center;
	float distance;
	int weight;

	Point( const Eigen::Vector3f &center, float distance, int weight = 1 ) : center( center ), distance( distance ), weight( weight ) {}
};

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

struct SparseCellInfo {
	Eigen::Vector3f minCorner;
	float resolution;

	// # potential/partial overlaps
	int upperBound;
	// # full overlaps
	int lowerBound;

	// debug-only
	//std::vector<int> fullPointIndices;
	std::vector<int> partialPointIndices;

	SparseCellInfo() : upperBound( 0 ), lowerBound( 0 ) {}
	SparseCellInfo( SparseCellInfo &&c ) : minCorner( c.minCorner), resolution( c.resolution ), upperBound( c.upperBound ),
		lowerBound( c.lowerBound ), /*fullPointIndices( std::move( c.fullPointIndices ) ),*/ partialPointIndices( std::move( c.partialPointIndices ) ) {}
};

SparseCellInfo buildRootCell( const std::vector<Point> &points, const float halfThickness ) {
	const Eigen::Vector3f minGridCorner = std::accumulate( points.cbegin(), points.cend(), points[0].center, [] (const Eigen::Vector3f &x, const Point &p) { return x.cwiseMin( p.center - Eigen::Vector3f::Constant( p.distance ) ); } ) - Eigen::Vector3f::Constant( halfThickness );
	const Eigen::Vector3f maxGridCorner = std::accumulate( points.cbegin(), points.cend(), points[0].center, [] (const Eigen::Vector3f &x, const Point &p) { return x.cwiseMax( p.center + Eigen::Vector3f::Constant( p.distance ) ); } ) + Eigen::Vector3f::Constant( halfThickness );

	const float gridResolution = (maxGridCorner - minGridCorner).maxCoeff();

	SparseCellInfo rootCell;
	rootCell.minCorner = minGridCorner;
	rootCell.resolution = gridResolution;

	rootCell.lowerBound = 0;
	rootCell.upperBound = points.size();

	rootCell.partialPointIndices.resize( points.size() );
	for( int i = 0 ; i < points.size() ; ++i ) {
		rootCell.partialPointIndices[i] = i;
	}

	return rootCell;
}

void filterCells( std::vector<SparseCellInfo> &cells, const std::function<bool(const SparseCellInfo &)> &remove_if ) {
	std::vector<SparseCellInfo> filteredCells;

	filteredCells.reserve( cells.size() );

	std::remove_copy_if( cells.begin(), cells.end(), std::back_inserter( filteredCells ), remove_if );
	std::swap( filteredCells, cells );
}

std::vector<SparseCellInfo> solveIntersectionsWithPriority( const std::vector<Point> &points, const float halfThickness, const float minResolution, int maxLowerBound = std::numeric_limits<int>::max(), int minUpperBound = 0 ) {
	SparseCellInfo rootCell = buildRootCell( points, halfThickness );	
	const int maxCellRadius = (int) ceil( rootCell.resolution / 2 / minResolution );
	const int minCellRadius = (int) floor( (rootCell.resolution / 2 - 2 * halfThickness) / minResolution );

	//const int oldMaxNumRefinementSteps = 8* maxCellRadius*maxCellRadius*maxCellRadius;
	// max num of filled cells: all points with same position and distance (max overlap) and #cells = spherical shell
	// not optimal with rounded results: 48533 estimated vs 185688 actually (radius 5, thickness 1, res 0.1)
	const int maxNumRefinementSteps = int( 4.0 * M_PI /3.0 * (maxCellRadius*maxCellRadius*maxCellRadius - minCellRadius*minCellRadius*minCellRadius) );

	// sorted ascending by upper bound	
	std::vector<SparseCellInfo> cells, finishedCells;
	cells.reserve( 1 + 7 * maxNumRefinementSteps );
	finishedCells.reserve( 1 + 7 * maxNumRefinementSteps );

	cells.push_back( std::move( rootCell ) );

	int bestLowerBound = 0;
	int formerBestLowerBound;

	for( int refinementStep = 0 ; refinementStep < maxNumRefinementSteps ; ++refinementStep ) {
		if( (refinementStep % 1000) == 0 ) {
			std::cout << refinementStep << "/" << maxNumRefinementSteps - 1 << "\n";
		}

		formerBestLowerBound = bestLowerBound;

		SparseCellInfo parentCell;
		// pop 
		std::swap( parentCell, cells.back() );
		cells.pop_back();

		// refine the cell (split into 8)
		const float resolution = parentCell.resolution / 2;
		for( int corner = 0 ; corner < 8 ; corner++ ) {			
			const Eigen::Vector3f offset = indexToCubeCorner[corner].cast<float>() * resolution;

			const Eigen::Vector3f minCorner = parentCell.minCorner + offset;
			const Eigen::Vector3f maxCorner = minCorner + Eigen::Vector3f::Constant( resolution );

			SparseCellInfo cell;

			cell.lowerBound = parentCell.lowerBound;
			// reset to lower bound before we test whether any partial overlap has become a full one
			cell.upperBound = parentCell.lowerBound;

			cell.partialPointIndices.reserve( parentCell.partialPointIndices.size() );
			
			for( int j = 0 ; j < parentCell.partialPointIndices.size() ; j++ ) {
				const int pointIndex = parentCell.partialPointIndices[j];
				const Point &point = points[pointIndex];

				const float minSquaredDistance = squaredMinDistanceAABoxPoint( minCorner, maxCorner, point.center );
				const float maxSquaredDistance = squaredMaxDistanceAABoxPoint( minCorner, maxCorner, point.center );

				float minSphereSquaredDistance = (point.distance - halfThickness) * (point.distance - halfThickness);
				float maxSphereSquaredDistance = (point.distance + halfThickness) * (point.distance + halfThickness);
				if( maxSquaredDistance < minSphereSquaredDistance || maxSphereSquaredDistance < minSquaredDistance ) {
					continue;
				}
				// at least partial
				cell.upperBound += point.weight;

				// full?
				if( minSphereSquaredDistance <= minSquaredDistance && maxSquaredDistance <= maxSphereSquaredDistance ) {
					cell.lowerBound += point.weight;
					//cell.fullPointIndices.push_back( pointIndex );
				}
				else {
					// only partial
					cell.partialPointIndices.push_back( pointIndex );
				}
			}

			// only keep cells that are a potential solution
			if( cell.upperBound >= bestLowerBound && cell.upperBound > 0 && cell.upperBound >= minUpperBound ) {
				cell.minCorner = minCorner;
				cell.resolution = resolution;

				bestLowerBound = std::max( bestLowerBound, cell.lowerBound );

				if( cell.upperBound == cell.lowerBound ) {
					finishedCells.push_back( cell );
				}
				else if( resolution <= minResolution || cell.lowerBound > maxLowerBound ) {
					finishedCells.push_back( cell );
				}
				else {
					cells.push_back( cell );
				}
			}
		}

		//intermediateResults.push_back( cells );

		// if the lower bound has changed filter all cells out with a worse upper bound
		if( formerBestLowerBound != bestLowerBound ) {
			filterCells( cells, [bestLowerBound](const SparseCellInfo &cell) { return cell.upperBound < bestLowerBound; } );
		}

		// we're done if no cells are left to refine here now
		if( cells.empty() ) {
			break;
		}

		// swap the cell with the best upper bound to the back
		{
			int bestIndex = 0;
			int bestUpperBound = bestLowerBound;
			for( int i = cells.size() - 1 ; i >= 0 ; --i ) {
				const int upperBound = cells[i].upperBound;

				if( upperBound >= bestUpperBound ) {
					bestUpperBound = upperBound;
					bestIndex = i;	

					if( upperBound == parentCell.upperBound ) {
						// this is the best we can achieve
						break;
					}
				}
			}
			if( bestIndex != cells.size() - 1 ) {
				std::swap( cells[bestIndex], cells.back() );
			}
		}
	}

	// filter the finishedCells
	filterCells( finishedCells, [minUpperBound, bestLowerBound](const SparseCellInfo &cell) { return cell.upperBound < bestLowerBound && cell.upperBound >= minUpperBound; } );	

	// move the remaining cells
	//std::move( cells.begin(), cells.end(), std::back_inserter( finishedCells ) );
	return finishedCells;
}