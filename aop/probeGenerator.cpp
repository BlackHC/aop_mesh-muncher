#include "probeGenerator.h"
#include "grid.h"
#include "boost/range/size.hpp"

#include "optixEigenInterop.h"
#include "boost/type_traits/extent.hpp"
#include "boost/format.hpp"

using namespace Eigen;

namespace {
	// 11 main axes
	const Eigen::Vector3i rotationAxes[] = {
		Eigen::Vector3i( 1, 0, 0 ), Eigen::Vector3i( 0, 1, 0 ), Eigen::Vector3i( 0, 0, 1 ), 
		Eigen::Vector3i( 1, 1, 0 ), Eigen::Vector3i( 1, -1, 0 ),
		Eigen::Vector3i( 1, 0, 1 ), Eigen::Vector3i( 1, 0, -1 ), 
		Eigen::Vector3i( 0, 1, -1 ), Eigen::Vector3i( 0, 1, 1 )
		//Eigen::Vector3i( 1, -1, 1 ), Eigen::Vector3i( 1, 1, -1 ),
		//Eigen::Vector3i( 1, 1, 1 ), Eigen::Vector3i( 1, -1, -1 ),
	};
}

namespace ProbeGenerator {
	static Eigen::Vector3f directions[ boost::extent< decltype( neighborOffsets ) >::value  ];
	BOOST_STATIC_ASSERT( boost::extent< decltype( neighborOffsets ) >::value  == 26 );

	static Eigen::Matrix3f orientations[ boost::extent< decltype( rotationAxes ) >::value * 3 + 1 ];
	static int rotatedDirectionsMap[ boost::extent< decltype( orientations ) >::value ][ boost::extent< decltype( neighborOffsets ) >::value ];

	void initDirections() {
		for( int i = 0 ; i < boost::size( neighborOffsets ) ; i++ ) {
			directions[i] = neighborOffsets[i].cast<float>().normalized();
		}
	}

	const Eigen::Vector3f &getDirection( int index ) {
		return directions[index];
	}

	const Eigen::Vector3f *getDirections() {
		return directions;
	}

	int getNumDirections() {
		return (int) boost::size( neighborOffsets );
	}

	void initOrientations() {
		// init the rotation matrices
		orientations[0].setIdentity();

		for( int i = 0 ; i < boost::size( rotationAxes ) ; ++i ) {
			const Vector3d rotationAxis = rotationAxes[i].cast<double>().normalized();
			orientations[ 1 + i * 3 ] = Eigen::AngleAxisd( Math::PI_2, rotationAxis ).toRotationMatrix().cast<float>();
			orientations[ 1 + i * 3 + 1 ] = Eigen::AngleAxisd( Math::PI, rotationAxis ).toRotationMatrix().cast<float>();
			orientations[ 1 + i * 3 + 2 ] = Eigen::AngleAxisd( Math::PI_2, -rotationAxis ).toRotationMatrix().cast<float>();
		}

		// map the directions
		for( int i = 0 ; i < boost::size( orientations ) ; ++i ) {
			//std::cout << "orientation: " << i << "\n";

			for( int j = 0 ; j < boost::size( directions ) ; ++j ) {
				const Vector3d rotatedDirection = (orientations[ i ] * directions[j]).cast<double>().normalized();

				// find the best direction that matches the rotated direction
				float bestCosAngle = -2.f;
				int bestMatchIndex = 0;
				for( int k = 0 ; k < boost::size( directions ) ; ++k ) {
					const float cosAngle = (float) rotatedDirection.dot( directions[ k ].cast<double>().normalized() );
					if( cosAngle > bestCosAngle ) {
						bestCosAngle = cosAngle;
						bestMatchIndex = k;
					}
				}
				//std::cout << boost::format( "\tdirection: %i cosAngle: %f -> %i (%f %f)\n" ) % j % bestCosAngle % bestMatchIndex % rotatedDirection.norm() % directions[ bestMatchIndex ].norm();
				rotatedDirectionsMap[ i ][ j ] = bestMatchIndex;
			}
		}
	}
	
	int getNumOrientations() {
		return (int) boost::size( orientations );
	}

	const int *getRotatedDirections( int orientationIndex ) {
		return rotatedDirectionsMap[ orientationIndex ];
	}

	const Eigen::Matrix3f getRotation( int orientationIndex ) {
		return orientations[ orientationIndex ];
	}

	static void transformProbe( const Probe &probe, const Obb::Transformation &transformation, Probe &transformedProbe ) {
		map( transformedProbe.direction ) = transformation.linear() * map( probe.direction );
		map( transformedProbe.position ) = transformation * map( probe.position );
	}

	void transformProbes( const std::vector<Probe> &probes,const Obb::Transformation &transformation,  std::vector<Probe> &transformedProbes ) {
		transformedProbes.resize( probes.size() );
		for( int probeIndex = 0 ; probeIndex < probes.size() ; probeIndex++ ) {
			transformProbe( probes[ probeIndex ], transformation, transformedProbes[ probeIndex ] );
		}
	}

	void generateRegularInstanceProbes( const Eigen::Vector3f &size, const float resolution, std::vector<Probe> &probes ) {
		const Vector3i probeCount3 = ceil( size / resolution + Eigen::Vector3f::Constant( 1.0f ) );

		// create the index<->position mapping
		// TODO: use createCenteredIndexMapping [9/26/2012 kirschan2]
		const auto indexMapping3 = createIndexMapping( probeCount3, -probeCount3.cast<float>() * resolution / 2, resolution );

		for( auto iterator3 = indexMapping3.getIterator() ; iterator3.hasMore() ; ++iterator3 ) {
			const Vector3f position = indexMapping3.getPosition( iterator3.getIndex3() );

			Probe probe;
			map( probe.position ) = position;

			for( int i = 0 ; i < boost::size( directions ) ; i++ ) {
				if( position.dot( directions[i] ) >= 0 ) {
					map( probe.direction ) = directions[i];
					probe.directionIndex = i;
					probes.push_back( probe );
				}
			}
		}
	}

	void generateQueryProbes( const Obb &obb, const float resolution, std::vector<Probe> &transformedProbes ) {
		const Vector3i probeCount3 = ceil( obb.size / resolution );

		// create the index<->position mapping
		const auto indexMapping3 = createIndexMapping( probeCount3, -probeCount3.cast<float>() * resolution / 2, resolution );

		transformedProbes.reserve( boost::size( directions ) * indexMapping3.count );

		for( auto iterator3 = indexMapping3.getIterator() ; iterator3.hasMore() ; ++iterator3 ) {
			const Vector3f position = indexMapping3.getPosition( iterator3.getIndex3() );

			Probe probe;
			map( probe.position ) = obb.transformation * position;

			for( int i = 0 ; i < boost::size( directions ) ; i++ ) {
				map( probe.direction ) = obb.transformation.linear() * directions[i];
				probe.directionIndex = i;
				transformedProbes.push_back( probe );
			}
		}
	}

	void appendProbesFromSample( const Eigen::Vector3f &position, const Eigen::Vector3f &averagedNormal, std::vector< Probe > &probes ) {
		Probe probe;
		map( probe.position ) = position;

		const float averagedNormalLength = averagedNormal.norm();
		const float threshold = averagedNormalLength * (averagedNormalLength - 1.0f);

		for( int i = 0 ; i < boost::size( directions ) ; i++ ) {
			if( averagedNormal.dot( directions[i] ) >= threshold ) {
				map( probe.direction ) = directions[i];
				probe.directionIndex = i;
				probes.push_back( probe );
			}
		}
	}
}