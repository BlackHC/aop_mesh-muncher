#pragma once

#include <Eigen/Eigen>

inline Eigen::Vector3i ceil( const Eigen::Vector3f &v ) {
	return Eigen::Vector3i( (int) ceil( v[0] ), (int) ceil( v[1] ), (int) ceil( v[2] ) );
}

inline Eigen::Vector3i floor( const Eigen::Vector3f &v ) {
	return Eigen::Vector3i( (int) floor( v[0] ), (int) floor( v[1] ), (int) floor( v[2] ) );
}

// xyz -120-> yzx
template< typename Vector >
Vector permute( const Vector &v, const int *permutation ) {
	return Vector( v[permutation[0]], v[permutation[1]], v[permutation[2]] );
}

// yzx -120->xyz
template< typename Vector >
Vector permute_reverse( const Vector &w, const int *permutation ) {
	Vector v;
	for( int i = 0 ; i < 3 ; ++i ) {
		v[ permutation[i] ] = w[i];	
	}
	return v;
}

inline Eigen::Matrix4f permutedToUnpermutedMatrix( const int *permutation ) {
	return (Eigen::Matrix4f() << Eigen::Vector3f::Unit( permutation[0] ), Eigen::Vector3f::Unit( permutation[1] ), Eigen::Vector3f::Unit( permutation[2] ), Eigen::Vector3f::Zero(), 0,0,0,1.0 ).finished();
}

inline Eigen::Matrix4f unpermutedToPermutedMatrix( const int *permutation ) {
	return (Eigen::Matrix4f() << Eigen::RowVector3f::Unit( permutation[0] ), 0.0,
		Eigen::RowVector3f::Unit( permutation[1] ), 0.0,
		Eigen::RowVector3f::Unit( permutation[2] ), 0.0,
		Eigen::RowVector4f::UnitW() ).finished();
}

/*
concept Index3 {
	@property const Eigen::Vector3i offset;
	@property const Eigen::Vector3i beginCorner, endCorner;
	@property const int count;

	int getIndex( const Eigen::Vector3i &index3 ) const;
	Eigen::Vector3i getIndex3( int index ) const;

	bool isValid( int index ) const;
	bool isValid( const Eigen::Vector3i &index3 ) const;

	Index3 permuted( const int permutation[3] ) const;
}
*/

template< typename conceptIndexer3 >
class IndexIterator3;

template< typename conceptIndexer3 >
class SubIndexIterator3;

// converts between a 3d index and a 1d index
struct SimpleIndexer3 {
	typedef IndexIterator3<SimpleIndexer3> Iterator;
	typedef SubIndexIterator3<SimpleIndexer3> SubIterator;

	Eigen::Vector3i size;
	int count;

	const Eigen::Vector3i & getSize() const {
		return size;
	}

	const Eigen::Vector3i getBeginCorner() const {
		return Eigen::Vector3i::Zero();
	}

	const Eigen::Vector3i &getEndCorner() const {
		return size;
	}
	
	SimpleIndexer3( const Eigen::Vector3i &size = Eigen::Vector3i::Zero() ) : size( size ), count( size.prod() ) {
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

	SimpleIndexer3 permuted( const int permutation[3] ) const {
		return SimpleIndexer3( permute( size , permutation ) );
	}

	bool isValid( int index ) const {
		return index >= 0 && index < count;
	}

	bool isValid( const Eigen::Vector3i &index3 ) const {
		for( int i = 0 ; i < 3 ; ++i ) {
			if( index3[i] < 0 || index3[i] >= size[i] ) {
				return false;
			}
		}
		return true;
	}

	Iterator getIterator() const;

	SubIterator getSubIterator( const Eigen::Vector3i &beginCorner, const Eigen::Vector3i &endCorner );
};

struct SubIndexer3 {
	typedef IndexIterator3<SubIndexer3> Iterator;
	typedef SubIndexIterator3<SubIndexer3> SubIterator;

	Eigen::Vector3i size;
	Eigen::Vector3i beginCorner, endCorner;
	int count;
	
	const Eigen::Vector3i & getSize() const {
		return size;
	}

	const Eigen::Vector3i &getBeginCorner() const {
		return beginCorner;
	}

	const Eigen::Vector3i &getEndCorner() const {
		return endCorner;
	}

	SubIndexer3() : count( 0 ), size( Eigen::Vector3i::Zero() ) {}

	SubIndexer3( const Eigen::Vector3i &beginCorner, const Eigen::Vector3i &endCorner ) {
		init( beginCorner, endCorner );
	}

	void init( const Eigen::Vector3i &beginCorner, const Eigen::Vector3i &endCorner ) {
		this->beginCorner = beginCorner;
		this->endCorner = endCorner;
		this->size = endCorner - beginCorner;
		count = size.prod();
	}

	// x, y, z -> |z|y|x|  
	int getIndex( const Eigen::Vector3i &index3 ) const {
		const Eigen::Vector3i offsetIndex3 = index3 - beginCorner;
		return offsetIndex3[0] + size[0] * (offsetIndex3[1] + size[1] * offsetIndex3[2]);
	}

	Eigen::Vector3i getIndex3( int index ) const {
		int x = index % size[0];
		index /= size[0];
		int y = index % size[1];
		index /= size[1];
		int z = index;
		return Eigen::Vector3i( x,y,z ) + beginCorner;
	}

	SubIndexer3 permuted( const int permutation[3] ) const {
		return SubIndexer3( permute( beginCorner, permutation ), permute( endCorner, permutation ) );
	}

	bool isValid( int index ) const {
		return index >= 0 && index < count;
	}

	bool isValid( const Eigen::Vector3i &index3 ) const {
		for( int i = 0 ; i < 3 ; ++i ) {
			if( index3[i] < beginCorner[i] || index3[i] >= endCorner[i] ) {
				return false;
			}
		}
		return true;
	}

	Iterator getIterator() const;

	SubIterator getSubIterator( const Eigen::Vector3i &beginCorner, const Eigen::Vector3i &endCorner );
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
template< typename conceptIndexer3 = SimpleIndexer3 >
class IndexIterator3 : public Index3Composition {
	const conceptIndexer3 &indexer;
	int index;

public:
	IndexIterator3( const conceptIndexer3 &indexer ) : indexer( indexer ), index( 0 ), Index3Composition( indexer.getBeginCorner() ) {
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

	IndexIterator3 &operator++() {
		++index;
		if( ++index3[0] >= indexer.getEndCorner()[0] ) {
			index3[0] = indexer.getBeginCorner()[0];
			if( ++index3[1] >= indexer.getEndCorner()[1] ) {
				index3[1] = indexer.getBeginCorner()[1];
				++index3[2];
			}
		}
		return *this;
	}

	bool rowHasMore() {
		return index3[0] < indexer.getEndCorner()[0] - 1;
	}

	bool sliceHasMore() {
		return index3[1] < indexer.getEndCorner()[1] - 1;
	}
};

SimpleIndexer3::Iterator SimpleIndexer3::getIterator() const {
	return Iterator( *this );
}

SubIndexer3::Iterator SubIndexer3::getIterator() const {
	return Iterator( *this );
}

// main use: iterating over an indexer. Overloading the * operator to return index is the logical shorthand operation
template< typename conceptIndexer3 = SimpleIndexer3 >
class SubIndexIterator3 : public Index3Composition {
	const conceptIndexer3 &indexer;
	Eigen::Vector3i beginCorner, endCorner;
	int index;

public:
	SubIndexIterator3( const conceptIndexer3 &indexer, const Eigen::Vector3i &beginCorner, const Eigen::Vector3i &endCorner ) : indexer( indexer ), Index3Composition( beginCorner ), beginCorner( beginCorner ), endCorner( endCorner ), index( indexer.getIndex( beginCorner ) ) {
	}

	bool hasMore() const {
		return index3[2] < endCorner[2];
	}

	const int & operator* () const {
		return index;
	}

	int getIndex() const {
		return index;
	}

	Eigen::Vector3i getSize() const {
		return endCorner - beginCorner;
	}

	SubIndexIterator3 &operator++() {
		++index;
		if( ++index3[0] >= endCorner[0] ) {
			index3[0] = beginCorner[0];
			index += indexer.getSize()[0] - getSize()[0];
			if( ++index3[1] >= endCorner[1] ) {
				index3[1] = beginCorner[1];
				++index3[2];
				index += indexer.getSize().head<2>().prod() - indexer.getSize()[0] * getSize()[1];
			}
		}
		return *this;
	}

	bool rowHasMore() {
		return index3[0] < endCorner[0] - 1;
	}

	bool sliceHasMore() {
		return index3[1] < endCorner[1] - 1;
	}
};

SimpleIndexer3::SubIterator SimpleIndexer3::getSubIterator( const Eigen::Vector3i &beginCorner, const Eigen::Vector3i &endCorner )
{
	return SubIterator( *this, beginCorner, endCorner );
}

SubIndexer3::SubIterator SubIndexer3::getSubIterator( const Eigen::Vector3i &beginCorner, const Eigen::Vector3i &endCorner )
{
	return SubIterator( *this, beginCorner, endCorner );
}

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

struct IndexMapping3 : SimpleIndexer3 {
	Eigen::Vector3f offset;
	float resolution;

	IndexMapping3( const Eigen::Vector3i &size = Eigen::Vector3i::Zero(), const Eigen::Vector3f &offset = Eigen::Vector3f::Zero(), float resolution = 0.0f ) : SimpleIndexer3( size ), offset( offset ), resolution( resolution ) {
	}

	void init( const Eigen::Vector3i &size, const Eigen::Vector3f &offset, float resolution ) {
		SimpleIndexer3::init( size );
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
	IndexMapping3 getSubGrid( const Eigen::Vector3i &offsetIndex, const Eigen::Vector3i &subSize, int subResolution ) const {
		return IndexMapping3( subSize / subResolution + Eigen::Vector3i::Constant(1), getPosition( offsetIndex ), resolution * subResolution );
	}
};

template< typename conceptIndexer3 = SimpleIndexer3 >
struct OrientedIndexMapping3 : conceptIndexer3 {
	typedef conceptIndexer3 Indexer3;

	Eigen::Affine3f indexToPosition;
	Eigen::Affine3f positionToIndex;

	OrientedIndexMapping3() {}

	OrientedIndexMapping3( const Indexer3 &indexer, const Eigen::Affine3f &indexToPosition ) : conceptIndexer3( indexer ), indexToPosition( indexToPosition ), positionToIndex( indexToPosition.inverse() ) {
	}

	// spans the same volume but with permuted coordinates
	// permutation specifies the source index ie permutedVector = vector[permutation[0]], vector[permutation[1]], vector[permutation[2]]
	OrientedIndexMapping3 permuted( const int permutation[3] ) const {
		const Eigen::Matrix4f permutedIndexToPosition = indexToPosition * /* permutedIndexToIndex */
			(Eigen::Matrix4f() << Eigen::Vector3f::Unit( permutation[0] ), Eigen::Vector3f::Unit( permutation[1] ), Eigen::Vector3f::Unit( permutation[2] ), Eigen::Vector3f::Zero(), 0,0,0,1.0 ).finished();
		return OrientedIndexMapping3( Indexer3::permuted( permutation ), Eigen::Affine3f( permutedIndexToPosition ) );
	}

	Eigen::Vector3f getPosition( const Eigen::Vector3i &index3 ) const {
		return indexToPosition * index3.cast<float>();
	}

	Eigen::Vector3f getInterpolatedPosition( const Eigen::Vector3f &index3f ) const {
		return indexToPosition * index3f;
	}

	// aka vector
	Eigen::Vector3f getDirection( const Eigen::Vector3f &indexVector ) const {
		return indexToPosition.linear() * indexVector;
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

	float getResolution() const {
		BOOST_ASSERT( indexToPosition(0,0) == indexToPosition(1,1) && indexToPosition(1,1) == indexToPosition(2,2) );
		return indexToPosition(0,0);
	}

	OrientedIndexMapping3<SubIndexer3> createSubMapping( const Eigen::Vector3i &beginCornerIndex, const Eigen::Vector3i &endCornerIndex ) const {
		return OrientedIndexMapping3<SubIndexer3>( SubIndexer3( beginCornerIndex, endCornerIndex ), indexToPosition );
	}

	OrientedIndexMapping3<SubIndexer3> createExpandedMapping( const float expansion ) const {
		Eigen::Vector3i cellExpansion = ceil( getIndexDirection( Eigen::Vector3f::Constant( expansion ) ) );
		return createSubMapping( -cellExpansion, Indexer3::getEndCorner() + cellExpansion );
	}

	OrientedIndexMapping3<> withBeginCornerAtOrigin() const {
		return OrientedIndexMapping3<SimpleIndexer3>( SimpleIndexer3( getSize() ), indexToPosition * Eigen::Translation3f( getBeginCorner().cast<float>() ) );
	}
};

inline OrientedIndexMapping3<> createOrientedIndexMapping( const Eigen::Vector3i &size, const Eigen::Vector3f &offset, const float resolution ) {
	Eigen::Affine3f indexToPosition = Eigen::Translation3f( offset ) * Eigen::Scaling( resolution );
	return OrientedIndexMapping3<>( SimpleIndexer3( size ) , indexToPosition );
}

inline OrientedIndexMapping3<> createOrientedIndexMapping( const IndexMapping3 &grid ) {
	return createOrientedIndexMapping( grid.size, grid.offset, grid.resolution );
}

typedef OrientedIndexMapping3<> SimpleOrientedIndexMapping3;
typedef OrientedIndexMapping3<SubIndexer3> SubOrientedIndexMapping;

#ifdef GRID_GTEST_UNIT_TESTS
#include "gtest.h"

using namespace Eigen;

static std::ostream & operator << (std::ostream & s, const Matrix3f & m) {
	return Eigen::operator<<( s, m );
}

static std::ostream & operator << (std::ostream & s, const Vector3f & m) {
	return Eigen::operator<<( s, m );
}

TEST( OrientedIndexMapping3, fromIdentity) {
	OrientedIndexMapping3<> grid = createOrientedIndexMapping( Vector3i::Constant(4), Vector3f::Zero(), 1.0 );
	EXPECT_EQ( grid.indexToPosition.matrix(), Matrix4f::Identity() );
	EXPECT_EQ( grid.getPosition( Vector3i::Zero() ), Vector3f::Zero() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitX() ), Vector3f::UnitX() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitY() ), Vector3f::UnitY() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitZ() ), Vector3f::UnitZ() );
}

TEST( OrientedIndexMapping3, fromIdentityWithOffset) {
	const Vector3f offset = Vector3f::Constant(1.0);
	const OrientedIndexMapping3<> grid = createOrientedIndexMapping( Vector3i::Constant(4), offset, 1.0 );
	EXPECT_EQ( grid.getPosition( Vector3i::Zero() ), offset );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitX() ), offset + Vector3f::UnitX() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitY() ), offset + Vector3f::UnitY() );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitZ() ), offset + Vector3f::UnitZ() );
}

TEST( OrientedIndexMapping3, fromOffsetAndResolution) {
	const Vector3f offset = Vector3f::Constant(1.0);
	const OrientedIndexMapping3<> grid = createOrientedIndexMapping( Vector3i::Constant(4), offset, 2.0 );
	EXPECT_EQ( grid.getPosition( Vector3i::Zero() ), offset );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitX() ), offset + Vector3f::UnitX() * 2 );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitY() ), offset + Vector3f::UnitY() * 2 );
	EXPECT_EQ( grid.getPosition( Vector3i::UnitZ() ), offset + Vector3f::UnitZ() * 2 );
}

TEST( OrientedIndexMapping3, permuted ) {
	const int permutation[] = {1,2,0};
	const Vector3f offset = Vector3f::Constant(1.0);
	const OrientedIndexMapping3<> grid = createOrientedIndexMapping( Vector3i::Constant(4), offset, 2.0 );
	const OrientedIndexMapping3<> permutedGrid = grid.permuted( permutation );

	EXPECT_EQ( permutedGrid.getPosition( Vector3i::Zero() ), offset );
	EXPECT_EQ( permutedGrid.getPosition( Vector3i::UnitZ() ), offset + Vector3f::UnitX() * 2 );
	EXPECT_EQ( permutedGrid.getPosition( Vector3i::UnitX() ), offset + Vector3f::UnitY() * 2 );
	EXPECT_EQ( permutedGrid.getPosition( Vector3i::UnitY() ), offset + Vector3f::UnitZ() * 2 );
}

static Vector4f h( const Vector3f &v ) {
	return (Vector4f() << v, 1.0).finished();
}

TEST( OrientedIndexMapping3, permuted_inverse ) {
	const int permutation[] = {1,2,0};
	const Vector3f offset = Vector3f::Constant(1.0);
	const OrientedIndexMapping3<> grid = createOrientedIndexMapping( Vector3i::Constant(4), offset, 2.0 );
	const OrientedIndexMapping3<> permutedGrid = grid.permuted( permutation );

	const Matrix4f inv = permutedGrid.positionToIndex.matrix();
	EXPECT_EQ( h( Vector3f::Zero() ), inv * h(offset) );
	EXPECT_EQ( h( Vector3f::UnitZ() ), inv * h(offset + Vector3f::UnitX() * 2) );
	EXPECT_EQ( h( Vector3f::UnitX() ), inv * h(offset + Vector3f::UnitY() * 2) );
	EXPECT_EQ( h( Vector3f::UnitY() ), inv * h(offset + Vector3f::UnitZ() * 2) );
}

TEST( OrientedIndexMapping3, subGridSimple ) {
	const OrientedIndexMapping3<> grid = createOrientedIndexMapping( Vector3i::Constant(4), Vector3f::Zero(), 2.0 );
	const auto expandedGrid = grid.createSubMapping( Vector3i::Constant( -2 ), Vector3i::Constant(6) );

	EXPECT_EQ( expandedGrid.getPosition( Vector3i::Zero() ), Vector3f::Zero() );
	EXPECT_EQ( expandedGrid.getPosition( Vector3i::Constant( -2 ) ), Vector3f::Constant( -4.0f ) );
}

TEST( OrientedIndexMapping3, expandGridSimple ) {
	const OrientedIndexMapping3<> grid = createOrientedIndexMapping( Vector3i::Constant(4), Vector3f::Zero(), 2.0 );
	const auto expandedGrid = grid.createExpandedMapping( 3.0 );

	EXPECT_EQ( expandedGrid.size, Vector3i::Constant( 8 ) );
	EXPECT_EQ( expandedGrid.getPosition( Vector3i::Zero() ), Vector3f::Zero() );
	EXPECT_EQ( expandedGrid.getPosition( Vector3i::Constant( -2 ) ), Vector3f::Constant( -4.0f ) );
}

TEST( Iterator3_Simple, coverageAndValidity ) {
	SimpleIndexer3 indexer( Vector3i( 3,5,7 ) );
	IndexIterator3<> iterator( indexer );

	for( int c = 0 ; c < 3*5*7 ; ++c ) {
		EXPECT_TRUE( indexer.isValid( *iterator ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex() ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex3() ) );
		ASSERT_TRUE( iterator.hasMore() );
		++iterator;
	}

	EXPECT_FALSE( iterator.hasMore() );
}

TEST( Iterator3_Volume, coverageAndValidity ) {
		SubIndexer3 indexer( Vector3i( -1, -2, -3 ),Vector3i( 2,3,4 ) );

		ASSERT_EQ( Vector3i( 3, 5, 7 ), indexer.getSize()  );

		IndexIterator3<SubIndexer3> iterator( indexer );

		for( int c = 0 ; c < 3*5*7 ; ++c ) {
			EXPECT_TRUE( indexer.isValid( *iterator ) );
			EXPECT_TRUE( indexer.isValid( iterator.getIndex() ) );
			EXPECT_TRUE( indexer.isValid( iterator.getIndex3() ) );
			ASSERT_TRUE( iterator.hasMore() );
			++iterator;
		}

		EXPECT_FALSE( iterator.hasMore() );
}

TEST( SubIterator3_Simple, simpleTest ) {
	SimpleIndexer3 indexer( Vector3i( 3,5,7 ) );
	SubIndexIterator3<> iterator( indexer, Vector3i( 1, 1, 1), Vector3i( 3, 3, 3 ) );

	ASSERT_EQ( Vector3i( 2, 2, 2 ), iterator.getSize()  );

	for( int c = 0 ; c < 2*2*2 ; ++c ) {
		EXPECT_TRUE( indexer.isValid( *iterator ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex() ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex3() ) );
		ASSERT_TRUE( iterator.hasMore() );
		++iterator;
	}

	EXPECT_FALSE( iterator.hasMore() );
}

TEST( SubIterator3_Volume, simpleTest ) {
	SubIndexer3 indexer( Vector3i( -1, -2, -3 ), Vector3i( 2,3,4 ) );
	SubIndexIterator3<SubIndexer3> iterator( indexer, Vector3i( 0, 1, 1), Vector3i( 2, 3, 3 ) );

	ASSERT_EQ( Vector3i( 2, 2, 2 ), iterator.getSize()  );

	for( int c = 0 ; c < 2*2*2 ; ++c ) {
		EXPECT_TRUE( indexer.isValid( *iterator ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex() ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex3() ) );
		ASSERT_TRUE( iterator.hasMore() );
		++iterator;
	}

	EXPECT_FALSE( iterator.hasMore() );
}

TEST( SubIterator3_Simple, coverageAndValidity ) {
	SimpleIndexer3 indexer( Vector3i( 3,5,7 ) );
	SubIndexIterator3<> iterator( indexer, Vector3i( 0, 0, 0), Vector3i( 3, 5, 7 ) );

	ASSERT_EQ( Vector3i( 3, 5, 7 ), iterator.getSize()  );

	for( int c = 0 ; c < 3*5*7 ; ++c ) {
		EXPECT_TRUE( indexer.isValid( *iterator ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex() ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex3() ) );
		ASSERT_TRUE( iterator.hasMore() );
		++iterator;
	}

	EXPECT_FALSE( iterator.hasMore() );
}

TEST( SubIterator3_Volume, coverageAndValidity ) {
	SubIndexer3 indexer( Vector3i( -1, -2, -3 ), Vector3i( 2,3,4 ) );
	SubIndexIterator3<SubIndexer3> iterator( indexer, Vector3i( -1, -2, -3), Vector3i( 2, 3, 4 ) );

	ASSERT_EQ( Vector3i( 3, 5, 7 ), iterator.getSize() );

	for( int c = 0 ; c < 3*5*7 ; ++c ) {
		EXPECT_TRUE( indexer.isValid( *iterator ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex() ) );
		EXPECT_TRUE( indexer.isValid( iterator.getIndex3() ) );
		ASSERT_TRUE( iterator.hasMore() );
		++iterator;
	}

	EXPECT_FALSE( iterator.hasMore() );
}

TEST( SubIterator3_Simple, index3ValueComparison ) {
	SimpleIndexer3 indexer( Vector3i( 3,5,7 ) );
	SubIndexIterator3<> iterator( indexer, Vector3i( 0, 0, 0), Vector3i( 3, 5, 7 ) );
	IndexIterator3<> refIterator( indexer );

	ASSERT_EQ( Vector3i( 3, 5, 7 ), iterator.getSize()  );

	for( int c = 0 ; c < 3*5*7 ; ++c ) {
		EXPECT_EQ( refIterator.getIndex3(), iterator.getIndex3() );
		ASSERT_TRUE( iterator.hasMore() );
		ASSERT_TRUE( refIterator.hasMore() );
		++iterator, ++refIterator;
	}

	EXPECT_FALSE( iterator.hasMore() );
	EXPECT_FALSE( refIterator.hasMore() );
}

TEST( SubIterator3_Volume, index3ValueComparison ) {
	SubIndexer3 indexer( Vector3i( -1, -2, -3 ), Vector3i( 2,3,4 ) );
	SubIndexIterator3<SubIndexer3> iterator( indexer, Vector3i( -1, -2, -3), Vector3i( 2, 3, 4 ) );
	IndexIterator3<SubIndexer3> refIterator( indexer );

	ASSERT_EQ( Vector3i( 3, 5, 7 ), iterator.getSize() );

	for( int c = 0 ; c < 3*5*7 ; ++c ) {
		EXPECT_EQ( refIterator.getIndex3(), iterator.getIndex3() );
		ASSERT_TRUE( iterator.hasMore() );
		ASSERT_TRUE( refIterator.hasMore() );
		++iterator, ++refIterator;
	}

	EXPECT_FALSE( iterator.hasMore() );
	EXPECT_FALSE( refIterator.hasMore() );
}

#endif