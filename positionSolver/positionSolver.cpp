#define _USE_MATH_DEFINES
#include <cmath>

#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <iterator>

#include <gtest.h>
#include <Eigen/Eigen>

#include <SFML/Window.hpp>

#include <memory>

namespace Math {
	inline float cotf( float radians ) {
		return std::tanf( M_PI_2 - radians );
	}
}

namespace Eigen {
// like glFrustum
Matrix4f createFrustumMatrix( const float left, const float right, const float bottom, const float top, const float near, const float far ) {
	const float width = right - left;
	const float height = top - bottom;
	const float depth = far - near;

	return (Matrix4f() <<
		2 * near / width,	0,					(right + left) / width,		0,
		0,					2 * near / height,	(top + bottom) / height,	0,
		0,					0,					-(far + near) / depth,		- 2 * far * near / depth,
		0,					0,					-1.0,						0).finished();
}

// like glOrtho
Matrix4f createOrthoMatrix( const float left, const float right, const float bottom, const float top, const float near, const float far ) {
	const float width = right - left;
	const float height = top - bottom;
	const float depth = far - near;

	return (Matrix4f() <<
		2 / width,		0,					0,				(right + left) / width,
		0,				2 / height,			0,				(top + bottom) / height,
		0,				0,					-2 / depth,		-(far + near) / depth,
		0,				0,					0,				-1.0).finished();
}

Matrix4f createPerspectiveMatrix( const float FoV_y, const float aspectRatio, const float zNear, const float zFar ) {
	const float f = Math::cotf( FoV_y / 2 );
	const float depth = zFar - zNear;

	return (Matrix4f() <<
		f / aspectRatio,	0, 0,						0,
		0,					f, 0,						0,
		0,					0, (zFar + zNear) / depth,	2 * zFar * zNear / depth,
		0,					0, -1.0,					0).finished();
}
}

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

std::vector<SparseCellInfo> solveIntersections( const std::vector<Point> &points, float halfThickness ) {
	// start with a resolution that guarantees that there will be full matches
	float resolution = halfThickness * 0.707106; // rounded down

	Vector3f minGridCorner = std::accumulate( points.cbegin(), points.cend(), points[0].center, [] (const Vector3f &x, const Point &p) { return x.cwiseMin( p.center - Vector3f::Constant( p.distance ) ); } );
	Vector3f maxGridCorner = std::accumulate( points.cbegin(), points.cend(), points[0].center, [] (const Vector3f &x, const Point &p) { return x.cwiseMax( p.center + Vector3f::Constant( p.distance ) ); } );

	Vector3i size = ((maxGridCorner - minGridCorner) / resolution + Vector3f::Constant(1)).cast<int>();
	Grid grid( size, minGridCorner, resolution );

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
		std::remove_copy_if( sparseCellInfos.begin(), sparseCellInfos.end(), std::back_inserter( filteredSparseCellInfos ), [bestFullCount](SparseCellInfo &info) { return info.totalCount < bestFullCount; } );
		std::swap( filteredSparseCellInfos, sparseCellInfos );
	}

	// done?
	if( bestFullCount == bestTotalCount ) {
		// solution found
		return sparseCellInfos;
	}

	// refine
	for( int refineStep = 0 ; refineStep < 8 ; ++refineStep ) {
		resolution /= 2;

		std::vector<SparseCellInfo> refinedSparseCellInfos;
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

		// filter
		if( bestFullCount > 0 ) {
			std::vector<SparseCellInfo> filteredSparseCellInfos;
			std::remove_copy_if( sparseCellInfos.begin(), sparseCellInfos.end(), std::back_inserter( filteredSparseCellInfos ), [bestFullCount](SparseCellInfo &info) { return info.totalCount < bestFullCount; } );
			std::swap( filteredSparseCellInfos, sparseCellInfos );
		}

		// done?
		if( bestFullCount == bestTotalCount ) {
			// solution found
			return sparseCellInfos;
		}
	}

	return sparseCellInfos;
}

struct Camera {
	const Eigen::Vector3f &getPosition() const { return position; }
	void setPosition(const Eigen::Vector3f &val) { position = val; }

	const Eigen::Quaternionf &getOrientation() const { return orientation; }
	void setOrientation(const Eigen::Quaternionf &val) { orientation = val; }

	Eigen::Matrix4f getViewMatrix() const {
		Eigen::Isometry3f viewTransformation = orientation * Eigen::Translation3f( -position );
		return viewTransformation.matrix();	
	}

	Eigen::Isometry3f getViewTransformation() const {
		Eigen::Isometry3f viewTransformation = orientation * Eigen::Translation3f( -position );
		return viewTransformation;
	}

	const Eigen::Matrix4f &getProjectionMatrix() const { return projectionMatrix; }
	void setProjectionMatrix(const Eigen::Matrix4f &val) { projectionMatrix = val; }
	
private:
	Eigen::Vector3f position;
	Eigen::Quaternionf orientation;

	Eigen::Matrix4f projectionMatrix;
};

struct EventHandler {
	// returns true if the event has been processed
	virtual bool handleEvent( const sf::Event &event ) { return false; }
	virtual bool update( const float elapsedTime, bool inputProcessed ) { return inputProcessed; }
};

struct EventDispatcher : public EventHandler {
	std::vector<std::shared_ptr<EventHandler>> eventHandlers;

	bool handleEvent( const sf::Event &event ) {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			if( eventHandler->get()->handleEvent( event ) ) {
				return true;
			}
		}
		return false;
	}

	bool update( const float elapsedTime, bool inputProcessed = false ) {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			inputProcessed |= eventHandler->get()->update( elapsedTime, inputProcessed );
		}
		return inputProcessed;
	}
};

struct null_deleter {
	template<typename T>
	void operator() (T*) {}
};

template<typename T>
std::shared_ptr<T> shared_from_stack(T &object) {
	return std::shared_ptr<T>( &object, null_deleter() );
}

struct CameraInputControl : public EventHandler {
	std::shared_ptr<Camera> camera;
	std::shared_ptr<sf::Window> window;
	sf::Vector2i lastMousePosition;
	bool active;
	
	void init( const std::shared_ptr<Camera> &camera, const std::shared_ptr<sf::Window> &window ) {
		this->camera = camera;
		this->window = window;
		lastMousePosition = sf::Mouse::getPosition();
		active = false;
	}

	bool handleEvent( const sf::Event &event ) {
		switch( event.type ) {
		case sf::Event::LostFocus:
			active = false;
			window->setMouseCursorVisible( true );
			break;
		case sf::Event::KeyPressed:
			if( event.key.code == sf::Keyboard::Escape ) {
				active = false;
				window->setMouseCursorVisible( true );
			}
			return true;
		case sf::Event::MouseButtonReleased:
			if( event.mouseButton.button == sf::Mouse::Button::Left ) {
				active = true;
				window->setMouseCursorVisible( false );
			}
			return true;
		}
		return false;
	}
	
	bool update( const float elapsedTime, bool inputProcessed ) {
		sf::Vector2i mousePosition = sf::Mouse::getPosition();

		if( !inputProcessed && active ) {
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
					
			relativeMovement *= elapsedTime;

			sf::Vector2i mouseMovement = mousePosition - lastMousePosition;
			camera->setPosition( camera->getPosition() + camera->getViewTransformation().linear() * relativeMovement );
			camera->setOrientation( Eigen::AngleAxisf( mouseMovement.x / M_PI, Eigen::Vector3f::Unit(1) ) *
				Eigen::AngleAxisf( mouseMovement.y / M_PI, Eigen::Vector3f::Unit(0) ) *
				camera->getOrientation() );

			sf::Mouse::setPosition( sf::Vector2i( window->getSize() / 2u ), *window );	
		}
		lastMousePosition = sf::Mouse::getPosition();

		return true;
	}
};

void main() {
	std::vector<Point> points;

	points.push_back( Point( Vector3f(0,0,0), 5 ) );
	points.push_back( Point( Vector3f(10,0,0), 5 ) );

	auto result = solveIntersections( points, 1 );

	sf::Window window( sf::VideoMode( 640, 480 ), "Position Solver" );

	Camera camera;
	CameraInputControl cameraInputControl;
	cameraInputControl.init( shared_from_stack(camera), shared_from_stack(window) );

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock;
	while (window.isOpen())
	{
		// Event processing
		sf::Event event;
		while (window.pollEvent(event))
		{
			// Request for closing the window
			if (event.type == sf::Event::Closed)
				window.close();

			cameraInputControl.handleEvent( event );
		}

		cameraInputControl.update( frameClock.restart().asSeconds(), false );

		// Activate the window for OpenGL rendering
		window.setActive();

		// OpenGL drawing commands go here...

		// End the current frame and display its contents on screen
		window.display();
	}

};