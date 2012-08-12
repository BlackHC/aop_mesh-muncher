#pragma once

#include <Eigen/Eigen>

// xyz 120 ->yzx
template< typename Vector >
Vector permute( const Vector &v, const int *permutation ) {
	return Vector( v[permutation[0]], v[permutation[1]], v[permutation[2]] );
}

// converts between a 3d index and a 1d index
struct Indexer3 {
	Eigen::Vector3i size;
	int count;

	Indexer3( const Eigen::Vector3i &size = Eigen::Vector3i::Zero() ) : size( size ), count( size.prod() ) {
	}

	void init( const Eigen::Vector3i &size ) {
		this->size = size;
		count = size.prod();
	}

	// x, y, z -> |z|y|x|  
	int getIndex( const Eigen::Vector3i &index3 ) const {
		return index3[0] + size[0] * (index3[1] + size[1] * index3[2]);
	}

	Eigen::Vector3i getIndex3( int index ) const {
		int x = index % size[0];
		index /= size[0];
		int y = index % size[1];
		index /= size[1];
		int z = index;
		return Eigen::Vector3i( x,y,z );
	}

	static Indexer3 fromPermuted( const Indexer3 &indexer, const int permutation[3] ) {
		return Indexer3( permute( indexer.size , permutation ) );
	}
};

// TODO: remove this again?
class Index3Composition {
protected:
	Eigen::Vector3i index3;

public:
	Index3Composition( const Eigen::Vector3i &index3 ) : index3( index3 ) {
	}

	const Eigen::Vector3i &getIndex3() const {
		return index3;
	}
};

// main use: iterating over an indexer. Overloading the * operator to return index is the logical shorthand operation
class Iterator3 : public Index3Composition {
	const Indexer3 &indexer;
	int index;

public:
	Iterator3( const Indexer3 &indexer ) : indexer( indexer ), index( 0 ), Index3Composition( Eigen::Vector3i::Zero() ) {
	}

	int getIndex() const {
		return index;
	}
	
	bool hasMore() const {
		return index < indexer.count;
	}

	int operator* () const {
		return getIndex();
	}

	Iterator3 &operator++() {
		++index;
		if( ++index3[0] >= indexer.size[0] ) {
			index3[0] = 0;
			if( ++index3[1] >= indexer.size[1] ) {
				index3[1] = 0;
				++index3[2];
			}
		}
		return *this;
	}
};

// main usage: iterating over a volume. Overloading the * operator to return index3 is the logical shorthand operation.
class VolumeIterator3 : public Index3Composition {
	Eigen::Vector3i beginCorner, endCorner;

public:
	VolumeIterator3( const Eigen::Vector3i &beginCorner, const Eigen::Vector3i &endCorner ) : Index3Composition( beginCorner ), beginCorner( beginCorner ), endCorner( endCorner ) {
	}

	bool hasMore() const {
		return index3[2] < endCorner[2];
	}

	const Eigen::Vector3i & operator* () const {
		return index3;
	}

	VolumeIterator3 &operator++() {
		if( ++index3[0] >= endCorner[0] ) {
			index3[0] = beginCorner[0];
			if( ++index3[1] >= endCorner[1] ) {
				index3[1] = beginCorner[1];
				++index3[2];
			}
		}
		return *this;
	}
};

struct Grid : Indexer3 {
	Eigen::Vector3f offset;
	float resolution;

	Grid( const Eigen::Vector3i &size = Eigen::Vector3i::Zero(), const Eigen::Vector3f &offset = Eigen::Vector3f::Zero(), float resolution = 0.0f ) : Indexer3( size ), offset( offset ), resolution( resolution ) {
	}

	void init( const Eigen::Vector3i &size, const Eigen::Vector3f &offset, float resolution ) {
		Indexer3::init( size );
		this->offset = offset;
		this->resolution = resolution;
	}

	Eigen::Vector3f getPosition( const Eigen::Vector3i &index3 ) const {
		return offset + index3.cast<float>() * resolution;
	}

	Eigen::Vector3f getIndexDirection( const Eigen::Vector3f &direction ) const {
		return direction / resolution;
	}

	Eigen::Vector3f getIndex3( const Eigen::Vector3f &position ) const {
		return (position - offset) / resolution;
	}

	// TODO: take a cube as parameter...
	// TODO: remove this again... very weird behavior...
	Grid getSubGrid( const Eigen::Vector3i &offsetIndex, const Eigen::Vector3i &subSize, int subResolution ) const {
		return Grid( subSize / subResolution + Eigen::Vector3i::Constant(1), getPosition( offsetIndex ), resolution * subResolution );
	}
};

struct OrientedGrid : Indexer3 {
	Eigen::Affine3f indexToPosition;
	Eigen::Affine3f positionToIndex;

	OrientedGrid() {}

	OrientedGrid( const Eigen::Vector3i &size, const Eigen::Affine3f &indexToPosition ) : Indexer3( size ), indexToPosition( indexToPosition ), positionToIndex( indexToPosition.inverse() ) {
	}

	static OrientedGrid from( const Eigen::Vector3i &size, const Eigen::Vector3f &offset, const float resolution ) {
		Eigen::Affine3f indexToPosition = Eigen::Translation3f( offset ) * Eigen::Scaling( resolution );
		return OrientedGrid( size, indexToPosition );
	}

	static OrientedGrid from( const Grid &grid ) {
		return from( grid.size, grid.offset, grid.resolution );
	}

	// spans the same volume but with permuted coordinates
	// permutation specifies the source index ie permutedVector = vector[permutation[0]], vector[permutation[1]], vector[permutation[2]]
	static OrientedGrid from( const OrientedGrid &originalGrid, const int permutation[3] ) {
		const Eigen::Vector3i permutedSize = permute( originalGrid.size, permutation );
		const Eigen::Matrix4f permutedIndexToPosition = originalGrid.indexToPosition * /* permutedIndexToIndex */
			(Eigen::Matrix4f() << Eigen::Vector3f::Unit( permutation[0] ), Eigen::Vector3f::Unit( permutation[1] ), Eigen::Vector3f::Unit( permutation[2] ), Eigen::Vector3f::Zero(), 0,0,0,1.0 ).finished();
		return OrientedGrid( permutedSize, Eigen::Affine3f( permutedIndexToPosition ) );
	}

	Eigen::Vector3f getPosition( const Eigen::Vector3i &index3 ) const {
		return indexToPosition * index3.cast<float>();
	}

	// aka vector
	Eigen::Vector3f getDirection( const Eigen::Vector3i &indexVector ) const {
		return indexToPosition.linear() * indexVector.cast<float>();
	}

	using Indexer3::getIndex3;

	// returns a float index, so you can decide which way to round
	Eigen::Vector3f getIndex3( const Eigen::Vector3f &position ) const {
		return positionToIndex * position;
	}

	Eigen::Vector3f getIndexDirection( const Eigen::Vector3f &vector ) const {
		return positionToIndex.linear() * vector;
	}

	Eigen::Vector3f getCenter() const {
		return indexToPosition * (size.cast<float>() * 0.5);
	}
};