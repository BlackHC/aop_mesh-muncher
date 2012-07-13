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

struct RayQuery {
	const Ray<Vector3f> ray;

	RayQuery( const Vector3f &start, const Vector3f &direction ) : ray( start, direction ) {}

	ConditionedVoxelType operator() (const Vector3i &_min, const Vector3i &_max) {
		return AxisAlignedBoundingBox3( _min.Cast<float>(), _max.Cast<float>() ).Trace( ray ) ? CVT_PARTIAL : CVT_NO_MATCH;
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

struct Cubei {
	Vector3i minCorner, maxCorner;

	Cubei( const Vector3i &minCorner, const Vector3i &maxCorner ) : minCorner( minCorner ), maxCorner( maxCorner ) {}

	static Cubei fromMinSize( const Vector3i &minCorner, const Vector3i &size ) {
		return Cubei( minCorner, minCorner + size );
	}

	Vector3i getSize() const {
		return maxCorner - minCorner;
	}
};

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

	Iterator3D getIteratorFromVolume( const Cubei &volume ) const {
		return Iterator3D( getIndex( volume.minCorner ), volume.getSize() / step );
	}

	Cubei getVolumeFromIndexCube( const Cubei &indexCube ) const {
		return Cubei( getPosition( indexCube.minCorner ), getPosition( indexCube.maxCorner ) );
	} 

	Vector3i getPosition( const Vector3i &index ) const {
		return VecCompMult( index, probeDims ) + min;
	}

	Vector3i getPosition( const Iterator3D &it ) const {
		return getPosition( it.ToVector() );
	}

	Vector3i getIndex( const Vector3i &position ) const {
		return (position - min) / step;
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

	bool validIndex( const Iterator3D &it ) const {
		return validIndex( it.ToVector() );
	}
};

typedef DataVolume<Probe> Probes;

const float PROBE_MAX_DELTA = 16;

// TODO: add weighted probes (probe weighted by inverse instance volume to be scale-invariant (more or less))
struct ProbeDatabase {
	int maxId;

	ProbeDatabase() : maxId( 0 ) {}

	void addProbe( const Probe &probe, int id ) {
		maxId = std::max( id, maxId );
		probeCountPerIdInstance.resize( maxId + 1 );

		probeIdMap.push_back( std::make_pair( probe, id ) );
	}

	typedef std::vector<int> CandidateIds;

	void initCandidateIds( CandidateIds &candidateIds ) const {
		candidateIds.resize( maxId );
	}

	void addCandidateIds( const Probe &probe, CandidateIds &candidateIds ) const {
		for( auto it = probeIdMap.cbegin() ; it != probeIdMap.cend() ; ++it ) {
			if( Probe::compare( it->first, probe ) <= PROBE_MAX_DELTA ) {
				++candidateIds[it->second];
			}
		}
	}

	int getProbeCountPerInstanceForId( int id ) const {
		return probeCountPerIdInstance[id];
	}

	typedef std::vector<std::pair<int, float>> WeightedCandidateIdVector;

	WeightedCandidateIdVector condenseCandidateIds( const CandidateIds &candidateIds ) const {
		WeightedCandidateIdVector candidates;

		for( int id = 0 ; id <= maxId ; ++id ) {
			if( candidateIds[id] ) {
				float score = float(candidateIds[id]) / getProbeCountPerInstanceForId(id);
				candidates.push_back( std::make_pair(id, score) );
			}
		}

		return candidates;
	}

	std::vector<std::pair<Probe, int>> probeIdMap;
	std::vector<int> probeCountPerIdInstance;
};

int getVolume( const Vector3i &size ) {
	return size.X() * size.Y() * size.Z();
}

ProbeDatabase::WeightedCandidateIdVector findCandidates( const Probes &probes, const ProbeDatabase &probeDatabase, const Cubei &targetVolume ) {
	ProbeDatabase::CandidateIds candidateIds;
	probeDatabase.initCandidateIds( candidateIds );

	for( Iterator3D it = probes.getIteratorFromVolume( targetVolume ) ; !it.IsAtEnd() ; ++it ) {
		if( probes.validIndex( it ) ) {
			probeDatabase.addCandidateIds( probes[ it ], candidateIds );
		}
	}

	ProbeDatabase::WeightedCandidateIdVector weightedCandidateIds = probeDatabase.condenseCandidateIds( candidateIds );
	typedef ProbeDatabase::WeightedCandidateIdVector::value_type ValuePair;

	std::sort( weightedCandidateIds.begin(), weightedCandidateIds.end(), []( const ValuePair &a, const ValuePair &b ) { return a.second > b.second; } );

	return weightedCandidateIds;
}

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

void addObjectInstanceToDatabase( Probes &probes, ProbeDatabase &database, const Cubei &instanceVolume, int id ) {
	for( Iterator3D it = probes.getIteratorFromVolume( instanceVolume ) ; !it.IsAtEnd() ; ++it ) {
		if( probes.validIndex( it ) ) {
			database.addProbe( probes[it], id );
		}
	}

	database.probeCountPerIdInstance[id] = getVolume( instanceVolume.getSize() );
}

void printCandidates( const ProbeDatabase::WeightedCandidateIdVector &candidates ) {
	std::cout << candidates.size() << " candidates\n";
	for( int i = 0 ; i < candidates.size() ; ++i ) {
		std::cout << "Weight: " << candidates[i].second << "\t\tId: " << candidates[i].first << "\n";
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

		sampleProbes( cache, probes );

		ProbeDatabase database;

		addObjectInstanceToDatabase( probes, database, probes.getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i(0,0,0), Vector3i(4,1,1) ) ), 0 );
		addObjectInstanceToDatabase( probes, database, probes.getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i(5,5,5), Vector3i(4,1,1) ) ), 1 );

		ProbeDatabase::WeightedCandidateIdVector result = findCandidates( probes, database, probes.getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i( 8, 8, 8 ), Vector3i( 8, 8, 8 ) ) ) );
		printCandidates( result );

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