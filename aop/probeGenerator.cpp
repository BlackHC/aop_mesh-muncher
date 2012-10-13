#include "probeGenerator.h"
#include "grid.h"
#include "boost/range/size.hpp"

#include "optixEigenInterop.h"

using namespace Eigen;

namespace ProbeGenerator {
	static Eigen::Vector3f directions[26];

	void initDirections() {
		for( int i = 0 ; i < boost::size( neighborOffsets ) ; i++ ) {
			directions[i] = neighborOffsets[i].cast<float>().normalized();
		}
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

	void generateInstanceProbes( const Eigen::Vector3f &size, const float resolution, std::vector<Probe> &probes ) {
		const Vector3i probeCount3 = ceil( size / resolution + Eigen::Vector3f::Constant( 1.0f ) );

		// create the index<->position mapping
		// TODO: use createCenteredIndexMapping [9/26/2012 kirschan2]
		const auto indexMapping3 = createIndexMapping( probeCount3, -probeCount3.cast<float>() * resolution / 2, resolution );

		for( auto iterator3 = indexMapping3.getIterator() ; iterator3.hasMore() ; ++iterator3 ) {
			const Vector3f position = indexMapping3.getPosition( iterator3.getIndex3() );

			Probe probe;
			map( probe.position ) = position;

			for( int i = 0 ; i < boost::size( directions ) ; i++ ) {
				if( position.dot( directions[i] ) > 0 ) {
					map( probe.direction ) = directions[i];
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
				transformedProbes.push_back( probe );
			}
		}
	}
}