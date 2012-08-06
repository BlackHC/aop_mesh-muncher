#pragma once

#include <Eigen/Eigen>

// xyz 120 ->yzx
template< typename Vector >
Vector permute( const Vector &v, const int *permutation ) {
	return Vector( v[permutation[0]], v[permutation[1]], v[permutation[2]] );
}

struct Indexer {
	Eigen::Vector3i size;
	int count;

	Indexer( const Eigen::Vector3i &size ) : size( size ), count( size.prod() ) {
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

	static Indexer fromPermuted( const Indexer &indexer, const int permutation[3] ) {
		return Indexer( permute( indexer.size , permutation ) );
	}
};

struct Grid : Indexer {
	Eigen::Vector3f offset;
	float resolution;

	Grid( const Eigen::Vector3i &size, const Eigen::Vector3f &offset, float resolution ) : Indexer( size ), offset( offset ), resolution( resolution ) {
	}

	Eigen::Vector3f getPosition( const Eigen::Vector3i &index3 ) const {
		return offset + index3.cast<float>() * resolution;
	}
};

struct OrientedGrid : Indexer {
	Eigen::Affine3f transformation;

	OrientedGrid( const Eigen::Vector3i &size, const Eigen::Affine3f &transformation ) : Indexer( size ), transformation( transformation ) {
	}

	static OrientedGrid from( const Eigen::Vector3i &size, const Eigen::Vector3f &offset, const float resolution ) {
		Eigen::Affine3f transformation = Eigen::Translation3f( offset ) * Eigen::Scaling( resolution );
		return OrientedGrid( size, transformation );
	}

	static OrientedGrid from( const Grid &grid ) {
		return from( grid.size, grid.offset, grid.resolution );
	}

	// spans the same grid but with permuted coordinates
	static OrientedGrid from( const OrientedGrid &originalGrid, const int permutation[3] ) {
		const Eigen::Vector3i permutedSize = permute( originalGrid.size, permutation );
		const Eigen::Matrix4f permutedTransformation = originalGrid.transformation * 
			(Eigen::Matrix4f() << Eigen::Vector3f::Unit( permutation[0] ), Eigen::Vector3f::Unit( permutation[1] ), Eigen::Vector3f::Unit( permutation[2] ), Eigen::Vector3f::Zero(), 0,0,0,1.0 ).finished();
		return OrientedGrid( permutedSize, Eigen::Affine3f( permutedTransformation ) );
	}

	Eigen::Vector3f getPosition( const Eigen::Vector3i &index3 ) const {
		return transformation * index3.cast<float>();
	}
};