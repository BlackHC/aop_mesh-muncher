#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <iterator>

#include <gtest.h>
#include <Eigen/Eigen>

#include "debugRender.h"

#include <SFML/Window.hpp>

#include <memory>

using namespace Eigen;

struct Point {
	Vector3f center;
	float distance;

	Point( const Vector3f &center, float distance ) : center( center ), distance( distance ) {}
};

struct Grid {
	Vector3i size;
	int count;

	Vector3f offset;
	float resolution;

	Grid( const Vector3i &size, const Vector3f &offset, float resolution ) : size( size ), offset( offset ), resolution( resolution ) {
		count = size[0] * size[1] * size[2];
	}

	int getIndex( const Vector3i &index3 ) {
		return index3[0] + size[0] * index3[1] + (size[0] * size[1]) * index3[2];
	}

	Vector3i getIndex3( int index ) {
		int x = index % size[0];
		index /= size[0];
		int y = index % size[1];
		index /= size[1];
		int z = index;
		return Vector3i( x,y,z );
	}

	Vector3f getPosition( const Vector3i &index3 ) {
		return offset + index3.cast<float>() * resolution;
	}
};

const Vector3i indexToCubeCorner[] = {
	Vector3i( 0,0,0 ), Vector3i( 1,0,0 ), Vector3i( 0,1,0 ), Vector3i( 0,0,1 ),
	Vector3i( 1,1,0 ), Vector3i( 0,1,1 ), Vector3i( 1,0,1 ), Vector3i( 1,1,1 )
};

inline float squaredMinDistanceAABoxPoint( const Vector3f &min, const Vector3f &max, const Vector3f &point ) {
	Vector3f distance;
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

inline float squaredMaxDistanceAABoxPoint( const Vector3f &min, const Vector3f &max, const Vector3f &point ) {
	Vector3f distanceA = (point - min).cwiseAbs();
	Vector3f distanceB = (point - max).cwiseAbs();
	Vector3f distance = distanceA.cwiseMax( distanceB );

	return distance.squaredNorm();
}

struct SparseCellInfo {
	Vector3f minCorner;
	Vector3f maxCorner;

	// upper bound
	int totalCount;
	// lower bound
	int fullCount;

	std::vector<int> fullPointIndices;
	std::vector<int> partialPointIndices;

	SparseCellInfo() : totalCount( 0 ), fullCount( 0 ) {}
	SparseCellInfo( SparseCellInfo &&c ) : minCorner( c.minCorner), maxCorner( c.maxCorner ), totalCount( c.totalCount ),
		fullCount( c.fullCount ), fullPointIndices( std::move( c.fullPointIndices ) ), partialPointIndices( std::move( c.partialPointIndices ) ) {}
};

std::vector<std::vector<SparseCellInfo>> solveIntersectionsFromPigeonLevel( const std::vector<Point> &points, float halfThickness ) {
	// advantage: starts at intermediate level
	// disadvantage: if there are many points, an additional acceleration structure is required for the first step
	std::vector<std::vector<SparseCellInfo>> results;

	Vector3f minGridCorner = std::accumulate( points.cbegin(), points.cend(), points[0].center, [] (const Vector3f &x, const Point &p) { return x.cwiseMin( p.center - Vector3f::Constant( p.distance ) ); } );
	Vector3f maxGridCorner = std::accumulate( points.cbegin(), points.cend(), points[0].center, [] (const Vector3f &x, const Point &p) { return x.cwiseMax( p.center + Vector3f::Constant( p.distance ) ); } );

#if 0
	float resolution = (maxGridCorner - minGridCorner).maxCoeff();
	Grid grid( Vector3i::Constant( 1 ), minGridCorner, resolution );
#else
	// start with a resolution that guarantees that there will be full matches
	float resolution = halfThickness * 0.707106; // rounded down
	Vector3i size = ((maxGridCorner - minGridCorner) / resolution + Vector3f::Constant(1)).cast<int>();
	Grid grid( size, minGridCorner, resolution );
#endif

	int bestFullCount = 0;
	int bestTotalCount = 0;

	std::vector<SparseCellInfo> sparseCellInfos;

	for( int i = 0 ; i < grid.count ; i++ ) {
		Vector3f minCorner = grid.getPosition( grid.getIndex3(i) );
		Vector3f maxCorner = minCorner + Vector3f::Constant( resolution );

		SparseCellInfo cellInfo;

		for( int pointIndex = 0 ; pointIndex < points.size() ; pointIndex++ ) {
			const Point &point = points[pointIndex];
			float minSquaredDistance = squaredMinDistanceAABoxPoint( minCorner, maxCorner, point.center );
			float maxSquaredDistance = squaredMaxDistanceAABoxPoint( minCorner, maxCorner, point.center );

			float minSphereSquaredDistance = (point.distance - halfThickness) * (point.distance - halfThickness);
			float maxSphereSquaredDistance = (point.distance + halfThickness) * (point.distance + halfThickness);
			if( maxSquaredDistance < minSphereSquaredDistance || maxSphereSquaredDistance < minSquaredDistance ) {
				// no intersection
				continue;
			}
			// at least partial
			cellInfo.totalCount++;

			// full?
			if( minSphereSquaredDistance <= minSquaredDistance && maxSquaredDistance <= maxSphereSquaredDistance ) {
				cellInfo.fullCount++;
				cellInfo.fullPointIndices.push_back(pointIndex);
			}
			else {
				// only partial
				cellInfo.partialPointIndices.push_back(pointIndex);
			}
		}

		// only keep potential solutions
		if( cellInfo.totalCount >= bestFullCount && cellInfo.totalCount > 0 ) {
			cellInfo.minCorner = minCorner;
			cellInfo.maxCorner = maxCorner;

			bestTotalCount = std::max( bestTotalCount, cellInfo.totalCount );
			bestFullCount = std::max( bestFullCount, cellInfo.fullCount );

			sparseCellInfos.push_back( std::move( cellInfo ) );
		}
	}

	// filter
	if( bestFullCount > 0 ) {
		std::vector<SparseCellInfo> filteredSparseCellInfos;
		filteredSparseCellInfos.reserve( sparseCellInfos.size() );

		std::remove_copy_if( sparseCellInfos.begin(), sparseCellInfos.end(), std::back_inserter( filteredSparseCellInfos ), [bestFullCount](SparseCellInfo &info) { return info.totalCount < bestFullCount; } );
		std::swap( filteredSparseCellInfos, sparseCellInfos );
	}

	// done?
	if( bestFullCount == bestTotalCount ) {
		// solution found
		results.push_back( std::move( sparseCellInfos ) );
		return results;
	}

	// refine
	for( int refineStep = 0 ; refineStep < 8 ; ++refineStep ) {
		resolution /= 2;

		std::vector<SparseCellInfo> refinedSparseCellInfos;
		refinedSparseCellInfos.reserve( sparseCellInfos.size() * 8 );

		// reset total count to lowest possible value (ie bestFullCount)
		bestTotalCount = bestFullCount;

		for( int i = 0 ; i < sparseCellInfos.size() ; ++i ) {
			const SparseCellInfo &parentCellInfo = sparseCellInfos[i];

			// early out if there is nothing to refine
			if( parentCellInfo.partialPointIndices.empty() ) {
				refinedSparseCellInfos.push_back( parentCellInfo );
				continue;
			}

			for( int k = 0 ; k < 8 ; k++ ) {
				Vector3f offset = indexToCubeCorner[k].cast<float>() * resolution;

				Vector3f minCorner = parentCellInfo.minCorner + offset;
				Vector3f maxCorner = minCorner + Vector3f::Constant( resolution );

				SparseCellInfo cellInfo;
				// full overlaps in the parent are also full overlaps in the refined child
				cellInfo.fullPointIndices = parentCellInfo.fullPointIndices;
				cellInfo.fullCount = parentCellInfo.fullCount;

				// reset total count to lowest possible value (ie bestFullCount)
				cellInfo.totalCount = cellInfo.fullCount;

				// check which partial overlaps become full overlaps in the refined child
				for( int j = 0 ; j < parentCellInfo.partialPointIndices.size() ; j++ ) {
					const int pointIndex = parentCellInfo.partialPointIndices[j];

					const Point &point = points[pointIndex];
					float minSquaredDistance = squaredMinDistanceAABoxPoint( minCorner, maxCorner, point.center );
					float maxSquaredDistance = squaredMaxDistanceAABoxPoint( minCorner, maxCorner, point.center );

					float minSphereSquaredDistance = (point.distance - halfThickness) * (point.distance - halfThickness);
					float maxSphereSquaredDistance = (point.distance + halfThickness) * (point.distance + halfThickness);
					if( maxSquaredDistance < minSphereSquaredDistance || maxSphereSquaredDistance < minSquaredDistance ) {
						continue;
					}
					// at least partial
					cellInfo.totalCount++;

					// full?
					if( minSphereSquaredDistance <= minSquaredDistance && maxSquaredDistance <= maxSphereSquaredDistance ) {
						cellInfo.fullCount++;
						cellInfo.fullPointIndices.push_back( pointIndex );
					}
					else {
						// only partial
						cellInfo.partialPointIndices.push_back( pointIndex );
					}
				}

				// only keep potential solutions
				if( cellInfo.totalCount >= bestFullCount && cellInfo.totalCount > 0 ) {
					cellInfo.minCorner = minCorner;
					cellInfo.maxCorner = maxCorner;

					bestTotalCount = std::max( bestTotalCount, cellInfo.totalCount );
					bestFullCount = std::max( bestFullCount, cellInfo.fullCount );

					refinedSparseCellInfos.push_back( std::move( cellInfo ) );
				}
			}
		}

		std::swap( sparseCellInfos, refinedSparseCellInfos );

		results.push_back( std::move( refinedSparseCellInfos ) );

		// filter
		if( bestFullCount > 0 ) {
			std::vector<SparseCellInfo> filteredSparseCellInfos;
			filteredSparseCellInfos.reserve( sparseCellInfos.size() );

			std::remove_copy_if( sparseCellInfos.begin(), sparseCellInfos.end(), std::back_inserter( filteredSparseCellInfos ), [bestFullCount](SparseCellInfo &info) { return info.totalCount < bestFullCount; } );
			std::swap( filteredSparseCellInfos, sparseCellInfos );
		}

		// done?
		if( bestFullCount == bestTotalCount ) {
			// solution found
			results.push_back( std::move( sparseCellInfos ) );
			return results;
		}
	}

	results.push_back( std::move( sparseCellInfos ) );
	return results;
}

std::vector<std::vector<SparseCellInfo>> solveIntersections( const std::vector<Point> &points, float halfThickness ) {
	// advantage: if there are many points, an additional acceleration structure is required for the first step
	// disadvantage: no meaning full results during the first (possibly many steps until resolution <= halfThickness * 0.707106)
	std::vector<std::vector<SparseCellInfo>> results;

	Vector3f minGridCorner = std::accumulate( points.cbegin(), points.cend(), points[0].center, [] (const Vector3f &x, const Point &p) { return x.cwiseMin( p.center - Vector3f::Constant( p.distance ) ); } );
	Vector3f maxGridCorner = std::accumulate( points.cbegin(), points.cend(), points[0].center, [] (const Vector3f &x, const Point &p) { return x.cwiseMax( p.center + Vector3f::Constant( p.distance ) ); } );

	float resolution = (maxGridCorner - minGridCorner).maxCoeff();
	//Grid grid( Vector3i::Constant( 1 ), minGridCorner, resolution );

	int bestFullCount = 0;
	int bestTotalCount = points.size();

	std::vector<SparseCellInfo> sparseCellInfos;

	SparseCellInfo gridCell;
	gridCell.minCorner = minGridCorner;
	gridCell.maxCorner = minGridCorner + Vector3f::Constant( resolution );
	gridCell.fullCount = 0;
	gridCell.totalCount = points.size();
	gridCell.partialPointIndices.resize( points.size() );
	for( int i = 0 ; i < points.size() ; ++i ) {
		gridCell.partialPointIndices[i] = i;
	}
	sparseCellInfos.push_back( gridCell );

	// refine
	for( int refineStep = 0 ; refineStep < 16 ; ++refineStep ) {
		resolution /= 2;

		std::vector<SparseCellInfo> refinedSparseCellInfos;
		refinedSparseCellInfos.reserve( sparseCellInfos.size() * 8 );

		// reset total count to lowest possible value (ie bestFullCount)
		bestTotalCount = bestFullCount;

		for( int i = 0 ; i < sparseCellInfos.size() ; ++i ) {
			const SparseCellInfo &parentCellInfo = sparseCellInfos[i];

			// early out if there is nothing to refine
			if( parentCellInfo.partialPointIndices.empty() ) {
				refinedSparseCellInfos.push_back( parentCellInfo );
				continue;
			}

			for( int k = 0 ; k < 8 ; k++ ) {
				Vector3f offset = indexToCubeCorner[k].cast<float>() * resolution;

				Vector3f minCorner = parentCellInfo.minCorner + offset;
				Vector3f maxCorner = minCorner + Vector3f::Constant( resolution );

				SparseCellInfo cellInfo;
				// full overlaps in the parent are also full overlaps in the refined child
				cellInfo.fullPointIndices = parentCellInfo.fullPointIndices;
				cellInfo.fullCount = parentCellInfo.fullCount;

				// reset total count to lowest possible value (ie bestFullCount)
				cellInfo.totalCount = cellInfo.fullCount;

				// check which partial overlaps become full overlaps in the refined child
				for( int j = 0 ; j < parentCellInfo.partialPointIndices.size() ; j++ ) {
					const int pointIndex = parentCellInfo.partialPointIndices[j];

					const Point &point = points[pointIndex];
					float minSquaredDistance = squaredMinDistanceAABoxPoint( minCorner, maxCorner, point.center );
					float maxSquaredDistance = squaredMaxDistanceAABoxPoint( minCorner, maxCorner, point.center );

					float minSphereSquaredDistance = (point.distance - halfThickness) * (point.distance - halfThickness);
					float maxSphereSquaredDistance = (point.distance + halfThickness) * (point.distance + halfThickness);
					if( maxSquaredDistance < minSphereSquaredDistance || maxSphereSquaredDistance < minSquaredDistance ) {
						continue;
					}
					// at least partial
					cellInfo.totalCount++;

					// full?
					if( minSphereSquaredDistance <= minSquaredDistance && maxSquaredDistance <= maxSphereSquaredDistance ) {
						cellInfo.fullCount++;
						cellInfo.fullPointIndices.push_back( pointIndex );
					}
					else {
						// only partial
						cellInfo.partialPointIndices.push_back( pointIndex );
					}
				}

				// only keep potential solutions
				if( cellInfo.totalCount >= bestFullCount && cellInfo.totalCount > 0 ) {
					cellInfo.minCorner = minCorner;
					cellInfo.maxCorner = maxCorner;

					bestTotalCount = std::max( bestTotalCount, cellInfo.totalCount );
					bestFullCount = std::max( bestFullCount, cellInfo.fullCount );

					refinedSparseCellInfos.push_back( std::move( cellInfo ) );
				}
			}
		}

		std::swap( sparseCellInfos, refinedSparseCellInfos );

		results.push_back( std::move( refinedSparseCellInfos ) );

		// filter
		if( bestFullCount > 0 ) {
			std::vector<SparseCellInfo> filteredSparseCellInfos;
			filteredSparseCellInfos.reserve( sparseCellInfos.size() );

			std::remove_copy_if( sparseCellInfos.begin(), sparseCellInfos.end(), std::back_inserter( filteredSparseCellInfos ), [bestFullCount](SparseCellInfo &info) { return info.totalCount < bestFullCount; } );
			std::swap( filteredSparseCellInfos, sparseCellInfos );
		}

		// done?
		if( bestFullCount == bestTotalCount ) {
			// solution found
			results.push_back( std::move( sparseCellInfos ) );
			return results;
		}
	}

	results.push_back( std::move( sparseCellInfos ) );
	return results;
}

#include "camera.h"
#include "eventHandling.h"

struct null_deleter {
	template<typename T>
	void operator() (T*) {}
};

template<typename T>
std::shared_ptr<T> shared_from_stack(T &object) {
	return std::shared_ptr<T>( &object, null_deleter() );
}

struct MouseCapture : public EventHandler  {
	typedef MouseCapture super;

	std::shared_ptr<sf::Window> window;

	MouseCapture() : captureMouse( false ) {}

	void init( const std::shared_ptr<sf::Window> &window ) {
		this->window = window;
	}
	
	bool getCapture() const {
		return captureMouse;
	}

	sf::Vector2i getMouseDelta() {
		sf::Vector2i temp = mouseDelta;
		mouseDelta = sf::Vector2i();
		return temp;
	}

	void setCapture( bool active ) {
		if( captureMouse == active ) {
			return;
		}

		mouseDelta = sf::Vector2i();

		if( active ) {
			oldMousePosition = sf::Mouse::getPosition();
		}
		// TODO: add support for ClipCursor?
		window->setMouseCursorVisible( !active );
		
		captureMouse = active;
	}

	bool handleEvent( const sf::Event &event ) {
		switch( event.type ) {
		case sf::Event::MouseLeft:
			if( captureMouse ) {
				sf::Mouse::setPosition( sf::Vector2i( window->getSize() / 2u ), *window );	
				oldMousePosition = sf::Mouse::getPosition();
			}
			return true;
		case sf::Event::MouseMoved:
			if( captureMouse ) {
				mouseDelta += sf::Mouse::getPosition() - oldMousePosition;
				oldMousePosition = sf::Mouse::getPosition();
			}			
			return true;
		}
		return false;
	}

private:
	sf::Vector2i mouseDelta, oldMousePosition;
	bool captureMouse;
};

struct CameraInputControl : public MouseCapture {
	std::shared_ptr<Camera> camera;

	void init( const std::shared_ptr<Camera> &camera, const std::shared_ptr<sf::Window> &window ) {
		super::init( window );
		this->camera = camera;
	}

	bool handleEvent( const sf::Event &event ) {
		if( super::handleEvent( event ) ) {
			return true;
		}

		switch( event.type ) {
		case sf::Event::LostFocus:
			setCapture( true );
			break;
		case sf::Event::KeyPressed:
			if( event.key.code == sf::Keyboard::Escape ) {
				setCapture( false );
			}
			return true;
		case sf::Event::MouseButtonReleased:
			if( event.mouseButton.button == sf::Mouse::Left ) {
				setCapture( true );
			}
			return true;
		}
		return false;
	}
	
	bool update( const float elapsedTime, bool inputProcessed ) {
		if( !inputProcessed && getCapture() ) {
			Eigen::Vector3f relativeMovement = Vector3f::Zero();
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::W ) ) {
				relativeMovement.z() -= 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::S ) ) {
				relativeMovement.z() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::A ) ) {
				relativeMovement.x() -= 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::D ) ) {
				relativeMovement.x() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::Space ) ) {
				relativeMovement.y() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::LControl ) ) {
				relativeMovement.y() -= 1;
			}
					
			relativeMovement *= elapsedTime * 10;

			Eigen::Vector3f newPosition = camera->getPosition() + camera->getViewTransformation().linear().transpose() * relativeMovement;
			camera->setPosition( newPosition );

			sf::Vector2f angleDelta = sf::Vector2f( getMouseDelta() ) * 0.5f;

			camera->yaw( angleDelta.x );
			camera->pitch( angleDelta.y );
		}

		return true;
	}
};

void main() {
	std::vector<Point> points;

	points.push_back( Point( Vector3f(0,0,0), 5 ) );
	points.push_back( Point( Vector3f(10,0,0), 5 ) );
	points.push_back( Point( Vector3f(5,0,5), 5 ) );

	points.push_back( Point( Vector3f(0,6,0), 5 ) );
	points.push_back( Point( Vector3f(10,6,0), 5 ) );
	points.push_back( Point( Vector3f(5,6,5), 5 ) );

	points.push_back( Point( Vector3f(0,6.5,0), 5 ) );
	points.push_back( Point( Vector3f(10,6.5,0), 5 ) );
	points.push_back( Point( Vector3f(5,6.5,5), 5 ) );

	/*points.push_back( Point( Vector3f(0,0,0), 5 ) );
	points.push_back( Point( Vector3f(11.8,0,0), 5 ) );*/

	const float halfThickness = 1;
	auto results = solveIntersections( points, halfThickness );

	sf::Window window( sf::VideoMode( 640, 480 ), "Position Solver", sf::Style::Default, sf::ContextSettings(32) );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 1.0;
	camera.perspectiveProjectionParameters.zFar = 500.0;
	//camera.position.z() = 5;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( shared_from_stack(camera), shared_from_stack(window) );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	DebugRender::CombinedCalls debugSourcePoints;
	debugSourcePoints.begin();
	// add source points
	for( auto point = points.begin() ; point != points.end() ; ++point ) {
		debugSourcePoints.setPosition( point->center );

		debugSourcePoints.setColor( Eigen::Vector3f( 0.0, 0.0, 1.0 ) );
		debugSourcePoints.drawWireframeSphere( point->distance + halfThickness );

		debugSourcePoints.setColor( Eigen::Vector3f( 0.0, 0.0, 0.5 ) );
		debugSourcePoints.drawWireframeSphere( point->distance - halfThickness );
	}
	debugSourcePoints.end();

	std::vector<DebugRender::CombinedCalls> debugResults;
	debugResults.resize( results.size() );
	int i = 0;
	for( auto sparseCellsLevel = results.begin() ; sparseCellsLevel != results.end() ; ++sparseCellsLevel, ++i ) {
		debugResults[i].begin();
		for( auto sparseCellInfo = sparseCellsLevel->begin() ; sparseCellInfo != sparseCellsLevel->end() ; ++sparseCellInfo ) {
			debugResults[i].setColor( Eigen::Vector3f( 1.0, 0.0, 0.0 ) * ( float(sparseCellInfo->fullCount) / sparseCellInfo->totalCount * 0.75 + 0.25 ) );
			debugResults[i].drawAABB( sparseCellInfo->minCorner, sparseCellInfo->maxCorner );
		}
		debugResults[i].end();
	}

	int level = results.size() - 1;

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;
	while (window.isOpen())
	{
		// Event processing
		sf::Event event;
		while (window.pollEvent(event))
		{
			// Request for closing the window
			if (event.type == sf::Event::Closed)
				window.close();

			if( event.type == sf::Event::Resized ) {
				camera.perspectiveProjectionParameters.aspect = float( event.size.width ) / event.size.height;
				glViewport( 0, 0, event.size.width, event.size.height );
			}

			cameraInputControl.handleEvent( event );

			if( event.type == sf::Event::KeyPressed ) {
				if( event.key.code == sf::Keyboard::Up ) {
					level = (level + 1) % results.size();
				}
				if( event.key.code == sf::Keyboard::Down ) {
					level = (level - 1 + results.size()) % results.size();
				}
			}
		}

		cameraInputControl.update( frameClock.restart().asSeconds(), false );

		// Activate the window for OpenGL rendering
		window.setActive();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// OpenGL drawing commands go here...
		glMatrixMode( GL_PROJECTION );
		glLoadMatrix( camera.getProjectionMatrix() );

		glMatrixMode( GL_MODELVIEW );
		glLoadMatrix( camera.getViewTransformation().matrix() );

		debugSourcePoints.render();
		debugResults[level].render();

		// End the current frame and display its contents on screen
		window.display();
	}

};