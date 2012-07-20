#pragma once

#include <niven.Core.Math.Vector.h>
#include <niven.Core.Math.VectorOperators.h>

#include "niven.Core.Geometry.Plane.h"

#include "niven.Core.MemoryLayout.h"
#include "niven.Core.Iterator3D.h"

#include "niven.Core.Math.VectorToString.h"

#include "niven.Engine.Sample.h"
#include "niven.Core.Math.3dUtility.h"
#include "niven.Engine.Spatial.AxisAlignedBoundingBox.h"
#include "niven.Core.Geometry.Ray.h"

#include <iostream>
#include <utility>

#include "findDistances.h"

// TODO: whatever...
using namespace niven;

template<typename V>
struct Cube {
	V minCorner, maxCorner;

	Cube() {}
	Cube( const V &minCorner, const V &maxCorner ) : minCorner( minCorner ), maxCorner( maxCorner ) {}

	static Cube fromMinSize( const V &minCorner, const V &size ) {
		return Cube( minCorner, minCorner + size );
	}

	V getSize() const {
		return maxCorner - minCorner;
	}
};

typedef Cube<Vector3i> Cubei;
typedef Cube<Vector3f> Cubef;

int getVolume( const Vector3i &size ) {
	return size.X() * size.Y() * size.Z();
}

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

struct RayQuery {
	const Ray<Vector3f> ray;

	RayQuery( const Vector3f &start, const Vector3f &direction ) : ray( start, direction ) {}

	ConditionedVoxelType operator() (const Vector3i &_min, const Vector3i &_max) {
		return Intersect( AxisAlignedBoundingBox3( _min.Cast<float>(), _max.Cast<float>() ), ray ) ? CVT_PARTIAL : CVT_NO_MATCH;
	}
};

// TODO: refactor from DataVolume
struct GridCoordinateMap {
	typedef Vector3i Vector;
	typedef Vector3i GridVector;

	int step;
	Vector min;
		
	Vector getPosition( const GridVector &index ) const {
		return index * step + min;
	}

	GridVector getFloorIndex( const Vector &position ) const {
		return (position - min) / step;
	}

	GridVector getCeilIndex( const Vector &position ) const {
		return (position + Vector3i::Constant( step - 1 ) - min) / step;
	}
};

struct UnorderedDistanceContext {
	static const int numSamples = 27;
	static Vector3f directions[numSamples];
	float sortedDistances[numSamples];
	float distances[numSamples];

	static void setDirections() {
		for( int i = 0 ; i < numSamples ; i++ ) {
			directions[i] = neighborOffsets[i].Cast<float>();
		}
	}

	void fill( DenseCache &cache, const Vector3i &volumePosition ) {
		for( int index = 0 ; index < numSamples ; ++index ) {
			RayQuery query( volumePosition.Cast<float>(), directions[index] );

			distances[index] = sortedDistances[index] = Math::Sqrt<float>( findSquaredDistanceToNearestConditionedVoxel( cache, volumePosition, query ) );
		}
		std::sort( sortedDistances, sortedDistances + numSamples );
	}

	double calculateAverage() const {
		double average = 0.;
		for( int index = 0 ; index < numSamples ; ++index ) {
			average += sortedDistances[index];
		}
		average /= numSamples;
		return average;
	}

	void normalizeWithAverage() {
		double average = calculateAverage();
		for( int index = 0 ; index < numSamples ; ++index ) {
			sortedDistances[index] /= average;
		}
	}

	static double compare( const UnorderedDistanceContext &a, const UnorderedDistanceContext &b, const float maxDistance ) {
		// use L2 norm because it accentuates big differences better than L1
		double norm = 0.;
		for( int index = 0 ; index < numSamples ; ++index ) {
			double delta = std::min( maxDistance, a.sortedDistances[index] ) - std::min( b.sortedDistances[index], maxDistance );
			norm = std::max( norm, Math::Abs(delta) );
		}
		return norm;
	}
};

Vector3f UnorderedDistanceContext::directions[UnorderedDistanceContext::numSamples];

typedef UnorderedDistanceContext Probe;
typedef std::vector<Probe> ProbeVector;

namespace Serialize {
	template<typename T>
	void writeTyped( FILE *fileHandle, const T &d ) {
		fwrite( &d, sizeof(T), 1, fileHandle );
	}

	template<typename T>
	void readTyped( FILE *fileHandle, T &d ) {
		fread( &d, sizeof(T), 1, fileHandle );
	}
}

template< typename Data >
struct DataVolume {
	typedef Cubei VolumeCube;
	typedef Cubei IndexCube;
	typedef Vector3i VolumeVector;
	typedef Vector3i IndexVector;

	VolumeVector min, size;
	int step;
	IndexVector probeDims;
	int probeCount;

	MemoryLayout3D layout;

	Data *data;

	DataVolume( VolumeVector min, VolumeVector size, int step ) : min( min ), size( size ), step( step ), probeDims( (size + Vector3i::Constant(step)) / step ), probeCount( probeDims.X() * probeDims.Y() * probeDims.Z() ), layout( probeDims ) {
		data = new Data[probeCount];
	}

	~DataVolume() {
		delete[] data;
	}
	
	IndexCube getIndexFromVolumeCube( const VolumeCube &volumeCube ) const {
		const Vector3i minVolumeIndex = getCeilIndex( volumeCube.minCorner );
		const Vector3i maxVolumeIndex = getFloorIndex( volumeCube.maxCorner );
		return IndexCube( minVolumeIndex, maxVolumeIndex );
	}

	VolumeCube getVolumeFromIndexCube( const IndexCube &indexCube ) const {
		return VolumeCube( getPosition( indexCube.minCorner ), getPosition( indexCube.maxCorner ) );
	} 

	Iterator3D getIterator() const { return Iterator3D(layout); }

	Iterator3D getIteratorFromVolume( const VolumeCube &volume ) const {
		const IndexCube	indexCube = getIndexFromVolumeCube( volume );
		return Iterator3D( indexCube.minCorner, indexCube.getSize() + Vector3i::Constant(1) );
	}
	
	VolumeVector getPosition( const IndexVector &index ) const {
		return index * step + min;
	}

	VolumeVector getPosition( const Iterator3D &it ) const {
		return getPosition( it.ToVector() );
	}

	VolumeVector getSize( const IndexVector &size ) const {
		return size * step;
	}

	IndexVector getFloorIndex( const VolumeVector &position ) const {
		return (position - min) / step;
	}

	IndexVector getCeilIndex( const VolumeVector &position ) const {
		return (position + Vector3i::Constant( step - 1 ) - min) / step;
	}


	Data& operator[] (const Iterator3D &it) {
		return get(it);
	}

	const Data& operator[] (const Iterator3D &it) const {
		return get(it);
	}

	Data& get(const Iterator3D &it) {
		return data[ layout( it.ToVector() ) ];
	}

	const Data& get(const Iterator3D &it) const {
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

	bool validIndex( const Iterator3D &it ) const {
		return validIndex( it.ToVector() );
	}

	void writeToFile( const char *file ) {
		using namespace Serialize;

		FILE *fileHandle = fopen( file, "wb" );
		writeTyped( fileHandle, min );
		writeTyped( fileHandle, size );
		writeTyped( fileHandle, step );
		writeTyped( fileHandle, probeCount );

		fwrite( data, sizeof(Data), probeCount, fileHandle );
		fclose( fileHandle );
	}

	bool readFromFile( const char *file ) {
		using namespace Serialize;

		FILE *fileHandle = fopen( file, "rb" );
		if( !fileHandle ) {
			std::cerr << "'" << file << "' could not be opened!\n";
			return false;
		}

		Vector3i headerMin, headerSize;
		int headerStep, headerProbeCount;
		readTyped( fileHandle, headerMin );
		readTyped( fileHandle, headerSize );
		readTyped( fileHandle, headerStep );
		readTyped( fileHandle, headerProbeCount );

		if( headerMin != min || headerSize != size || headerStep != step || headerProbeCount != probeCount ) {
			fclose( fileHandle );
			return false;
		}

		if( fread( data, sizeof(Data), probeCount, fileHandle ) != probeCount ) {
			fclose( fileHandle );
			return false;
		}

		fclose( fileHandle );
		return true;
	}
};

typedef DataVolume<Probe> Probes;

const float PROBE_MAX_DELTA = 16;

// TODO: add weighted probes (probe weighted by inverse instance volume to be scale-invariant (more or less))
struct ProbeDatabase {
	int numIds;

	ProbeDatabase() : numIds( 0 ) {}

	void addObjectInstanceToDatabase( Probes &probes, const Probes::VolumeCube &instanceVolume, int id ) {
		numIds = std::max( id + 1, numIds );
		probeCountPerIdInstance.resize( numIds );

		int count = 0;
		for( Iterator3D it = probes.getIteratorFromVolume( instanceVolume ) ; !it.IsAtEnd() ; ++it, ++count ) {
			if( probes.validIndex( it ) ) {
				probeIdMap.push_back( std::make_pair( probes[ it ], id ) );
			}
		}

		probeCountPerIdInstance[id] = count;
	}

	struct CandidateInfo {
		float score;

		// for the score
		int totalMatchCount;
		int maxSingleMatchCount;

		std::vector< std::pair<Probes::IndexVector, int> > matches;

		CandidateInfo() : totalMatchCount(0), maxSingleMatchCount(0), score(0) {}
	};

	typedef std::vector<CandidateInfo> CandidateInfos;
	typedef std::vector< std::pair<int, CandidateInfo > > SparseCandidateInfos;

	SparseCandidateInfos findCandidates( const Probes &probes, const Probes::VolumeCube &targetVolume, float maxDistance ) {
		CandidateInfos candidateInfos( numIds );

		for( Iterator3D targetIterator = probes.getIteratorFromVolume( targetVolume ) ; !targetIterator.IsAtEnd() ; ++targetIterator ) {
			if( probes.validIndex( targetIterator ) ) {
				const Probe &probe = probes[ targetIterator ];
				std::vector<int> matchCounts( numIds );

				for( auto refIterator = probeIdMap.cbegin() ; refIterator != probeIdMap.cend() ; ++refIterator ) {
					if( Probe::compare( refIterator->first, probe, maxDistance ) <= PROBE_MAX_DELTA ) {
						++matchCounts[refIterator->second];
					}
				}				

				for( int id = 0 ; id < numIds ; ++id ) {
					const int matchCount = matchCounts[id];
					if( matchCount == 0 ) {
						continue;
					}
					
					CandidateInfo &candidateInfo = candidateInfos[id];
					candidateInfo.totalMatchCount += matchCount;
					candidateInfo.matches.push_back( std::make_pair( targetIterator.ToVector(), matchCount ) );
					candidateInfo.maxSingleMatchCount = std::max( candidateInfo.maxSingleMatchCount, matchCount );
				}
			}
		}

		SparseCandidateInfos results;
		for( int id = 0 ; id < numIds ; ++id ) {
			CandidateInfo &candidateInfo = candidateInfos[id];

			size_t matchCount = candidateInfo.totalMatchCount;
			if( matchCount > 0 ) {
				candidateInfo.score = float(matchCount) / getProbeCountPerInstanceForId(id);

				typedef decltype(candidateInfo.matches[0]) value_type;
				std::sort( candidateInfo.matches.begin(), candidateInfo.matches.end(), [](const value_type &a, const value_type b) { return a.second > b.second; } );

				results.push_back( std::make_pair( id, std::move( candidateInfo ) ) );
			}
		}

		std::sort( results.begin(), results.end(), []( const ProbeDatabase::SparseCandidateInfos::value_type &a, const ProbeDatabase::SparseCandidateInfos::value_type &b ) { return a.second.score > b.second.score; } );

		return results;
	}

	int getProbeCountPerInstanceForId( int id ) const {
		return probeCountPerIdInstance[id];
	}

	// probe->id
	std::vector<std::pair<Probe, int>> probeIdMap;
	std::vector<int> probeCountPerIdInstance;
};

void sampleProbes( DenseCache &cache, Probes &probes ) {
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
}

void printCandidates( std::ostream &out, const ProbeDatabase::SparseCandidateInfos &candidates ) {
	out << candidates.size() << " candidates\n";
	for( int i = 0 ; i < candidates.size() ; ++i ) {
		out << "Weight: " << candidates[i].second.score << "\t\tId: " << candidates[i].first << "\n";
	}
}