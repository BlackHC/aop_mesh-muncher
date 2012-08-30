#pragma once
#include "grid.h"

template< typename Data, typename OrientedIndexMapping3 = SimpleOrientedIndexMapping3 >
class GridStorage : public boost::noncopyable {
	OrientedIndexMapping3 mapping;
	std::unique_ptr<Data[]> data;

public:
	GridStorage() {}

	GridStorage( const OrientedIndexMapping3 &mapping, Data *data ) : mapping( mapping ) {
		this->data.reset( data );
	}

	// move constructor
	GridStorage( GridStorage &&other ) : mapping( std::move( other.grid ) ), data( std::move( other.data ) ) {}

	GridStorage & operator = ( GridStorage &&other ) {
		mapping = std::move( other.grid );
		data = std::move( other.data );

		return *this;
	}

	GridStorage( const OrientedIndexMapping3 &mapping ) {
		reset( mapping );
	}

	void reset( const OrientedIndexMapping3 &mapping ) {
		this->mapping = mapping;
		data.reset( new Data[ mapping.count ] );
	}

	const OrientedIndexMapping3 & getMapping() const {
		return mapping;
	}

	typename OrientedIndexMapping3::Iterator getIterator() const {
		return OrientedIndexMapping3::Iterator( mapping );
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
};