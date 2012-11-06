#include "probeGenerator.h"
#include "grid.h"
#include "boost/range/size.hpp"

#include "optixEigenInterop.h"
#include "boost/type_traits/extent.hpp"
#include "boost/format.hpp"
#include <mathUtility.h>

using namespace Eigen;

namespace ProbeGenerator {
	static Eigen::Vector3f directions[ boost::extent< decltype( neighborOffsets ) >::value  ];
	BOOST_STATIC_ASSERT( boost::extent< decltype( neighborOffsets ) >::value  == 26 );

	static Eigen::Matrix3f orientations[ 24 ];
	static int rotatedDirectionsMap[ 24 ][ 26 ];

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
		int orientationIndex = 0;
		for( int firstAxis = 0 ; firstAxis < 3 ; firstAxis++ ) {
			for( int firstSign = 0 ; firstSign < 2 ; firstSign++ ) {
				const Vector3f firstDirection = Vector3f::Unit( firstAxis ) * ((firstSign > 0) ? -1.0 : 1.0);
				for( int secondAxis = 0 ; secondAxis < 2 ; secondAxis++) {
					for( int secondSign = 0 ; secondSign < 2 ; secondSign++ ) {
						const Vector3f secondDirection = Vector3f::Unit( (firstAxis + secondAxis + 1) % 3 ) * ((secondSign > 0) ? -1.0 : 1.0);
						const Vector3f thirdDirection = firstDirection.cross( secondDirection );

						orientations[ orientationIndex ].col( 0 ) = firstDirection;
						orientations[ orientationIndex ].col( 1 ) = secondDirection;
						orientations[ orientationIndex ].col( 2 ) = thirdDirection;

						orientationIndex++;
					}
				}
			}
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

	void transformProbe(
		const Probe &probe,
		const Obb::Transformation &transformation,
		const float resolution,
		TransformedProbe &transformedProbe
	) {
		map( transformedProbe.direction ) = transformation.linear() * getDirection( probe.directionIndex );
		map( transformedProbe.position ) = transformation * (probe.position.cast<float>() * resolution);
	}

	void transformProbes(
		const std::vector<Probe> &probes,
		const Obb::Transformation &transformation,
		const float resolution,
		TransformedProbes &transformedProbes
	) {
		transformedProbes.resize( probes.size() );
		for( int probeIndex = 0 ; probeIndex < probes.size() ; probeIndex++ ) {
			transformProbe( probes[ probeIndex ], transformation, resolution, transformedProbes[ probeIndex ] );
		}
	}

	Eigen::Vector3i getGridHalfExtent( const Eigen::Vector3f &size, const float resolution ) {
		return ceil( size / (2.f * resolution) - Vector3f::Constant( 0.5f ) );
	}

	void generateRegularInstanceProbes(
		const Eigen::Vector3f &size,
		const float resolution,
		Probes &probes
	) {
		const Vector3i halfProbeExtent = getGridHalfExtent( size, resolution );

		if( halfProbeExtent.maxCoeff() > 127 ) {
			throw std::logic_error( "grid is too big, use a coarser resolution!" );
		}

		const Vector3i probeCount3 = 2 * halfProbeExtent + Vector3i::Constant( 1 );
		probes.reserve( probeCount3.prod() );

		Probe probe;
		for( int directionIndex = 0 ; directionIndex < boost::size( directions ) ; directionIndex++ ) {
			const auto direction = directions[ directionIndex ];

			for( int z = -halfProbeExtent.z() ; z <= halfProbeExtent.z() ; z++ ) {
				probe.position.z() = z;

				for( int y = -halfProbeExtent.y() ; y <= halfProbeExtent.y() ; y++ ) {
					probe.position.y() = y;

					for( int x = -halfProbeExtent.x() ; x <= halfProbeExtent.x() ; x++ ) {
						probe.position.x() = x;

						if( direction.dot( probe.position.cast<float>() ) >= 0.0f ) {
							probe.directionIndex = directionIndex;
							probes.push_back( probe );
						}
					}
				}
			}
		}
	}

	void generateQueryProbes(
		const Eigen::Vector3f &size,
		const float resolution,
		Probes &probes
	) {
		const Vector3i halfProbeExtent = getGridHalfExtent( size, resolution );

		if( halfProbeExtent.maxCoeff() > 127 ) {
			throw std::logic_error( "grid is too big, use a coarser resolution!" );
		}

		const Vector3i probeCount3 = 2 * halfProbeExtent + Vector3i::Constant( 1 );
		probes.reserve( probeCount3.prod() );

		Probe probe;
		for( int directionIndex = 0 ; directionIndex < boost::size( directions ) ; directionIndex++ ) {
			for( int z = -halfProbeExtent.z() ; z <= halfProbeExtent.z() ; z++ ) {
				probe.position.z() = z;

				for( int y = -halfProbeExtent.y() ; y <= halfProbeExtent.y() ; y++ ) {
					probe.position.y() = y;

					for( int x = -halfProbeExtent.x() ; x <= halfProbeExtent.x() ; x++ ) {
						probe.position.x() = x;

						probe.directionIndex = directionIndex;
						probes.push_back( probe );
					}
				}
			}
		}
	}

	void appendProbesFromSample(
		const float resolution,
		const Eigen::Vector3f &position,
		const Eigen::Vector3f &averagedNormal,
		Probes &probes
	) {
		Probe probe;

		const Vector3i cellPosition = floor( position / resolution + Vector3f::Constant( 0.5f ) );
		probe.position = cellPosition.cast< signed char >();

		const float averagedNormalLength = averagedNormal.norm();
		const float threshold = averagedNormalLength * (averagedNormalLength - 1.0f);

		for( int directionIndex = 0 ; directionIndex< boost::size( directions ) ; directionIndex++ ) {
			if( averagedNormal.dot( directions[ directionIndex ] ) >= threshold ) {
				probe.directionIndex = directionIndex;
				probes.push_back( probe );
			}
		}
	}

	ProbePositions rotateProbePositions( const Probes &probes, int orientationIndex ) {
		const Matrix3f rotation = getRotation( orientationIndex );

		ProbePositions probePositions;
		probePositions.reserve( probes.size() );

		const int probesCount = (int) probes.size();
		for( int probeIndex = 0 ; probeIndex < probesCount ; probeIndex++ ) {
			const auto &probe = probes[ probeIndex ];

			const Vector3f rotatedPosition = rotation * probe.position.cast<float>();
			probePositions.push_back( floor(rotatedPosition + Vector3f::Constant( 0.5f )).cast<signed char>() );
		}

		return probePositions;
	}
}