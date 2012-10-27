#pragma once
#include "grid.h"

#include <vector>

template< typename Data, typename IndexMapping3 = SimpleIndexMapping3 >
class GridStorage {
public:
	GridStorage() {}

	// GridStorage will 'own' the data memory (and delete it on destruction)
	GridStorage( const IndexMapping3 &mapping, Data *data ) : mapping( mapping ) {
		this->data.resize( mapping.count );
		this->data.assign( data, data + mapping.count );
		delete [] data;
	}

	// move constructor
	GridStorage( GridStorage &&other )
		: mapping( std::move( other.mapping ) )
		, data( std::move( other.data ) ) {}

	GridStorage & operator = ( GridStorage &&other ) {
		mapping = std::move( other.mapping );
		data = std::move( other.data );

		return *this;
	}

	GridStorage( const IndexMapping3 &mapping ) {
		reset( mapping );
	}

	void reset( const IndexMapping3 &mapping ) {
		this->mapping = mapping;
		data.clear();
		data.resize( mapping.count );
	}

	const IndexMapping3 & getMapping() const {
		return mapping;
	}

	typename IndexMapping3::Iterator getIterator() const {
		return IndexMapping3::Iterator( mapping );
	}

	Data * getData() {
		return &data.front();
	}

	const Data * getData() const {
		return &data.front();
	}

	Data & operator[] ( const int index ) {
		return data[ index ];
	}

	Data & operator[] ( const Eigen::Vector3i &index3 ) {
		return data[ mapping.getIndex( index3 ) ];
	}

	const Data & operator[] ( const int index ) const {
		return data[ index ];
	}

	const Data & operator[] ( const Eigen::Vector3i &index3 ) const {
		return data[ mapping.getIndex( index3 ) ];
	}

	Data & get( const int index ) {
		return data[ index ];
	}

	Data & get( const Eigen::Vector3i &index3 ) {
		return data[ mapping.getIndex( index3 ) ];
	}

	const Data & get( const int index ) const {
		return data[ index ];
	}

	const Data & get( const Eigen::Vector3i &index3 ) const {
		return data[ mapping.getIndex( index3 ) ];
	}

	Data tryGet( const Eigen::Vector3i &index3 ) const {
		if( mapping.isValid( index3 ) ) {
			return get( index3 );
		}
		else {
			return Data();
		}
	}

#if 1
private:
	// better error messages than with boost::noncopyable
	GridStorage( const GridStorage &other );
	GridStorage & operator = ( const GridStorage &other );
#endif

	// I don't want to include the serializer here, so make it public for now
	// TODO: fix this [10/13/2012 kirschan2]
public:
	IndexMapping3 mapping;
	std::vector< Data > data;
};