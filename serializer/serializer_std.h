#pragma once

#include "serializer.h"

#include <vector>
#include <map>
#include <string>
#include <list>

#include <boost/type_traits/is_fundamental.hpp>

namespace Serializer {
	// std::vector
	template< typename Value >
	void write( BinaryWriter &writer, const std::vector<Value> &collection ) {
		unsigned int size = (unsigned int) collection.size();
		write( writer, size );

		if( !detail::can_be_dumped<Value>::value ) {
			for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
				write( writer, *it );
			}
		}
		else {
			// speed up fundamental types or raw types :)
			fwrite( &collection.front(), sizeof( Value ), size, writer.handle );
		}
	}

	template< typename Value >
	void read( BinaryReader &reader, std::vector<Value> &collection ) {
		unsigned int size;
		read( reader, size );

		unsigned int startIndex = (unsigned int) collection.size();
		collection.reserve( startIndex + size );

		if( !detail::can_be_dumped<Value>::value ) {
			for( unsigned int i = 0 ; i < size ; ++i ) {
				Value value;
				read( reader, value );
				collection.push_back( std::move( value ) );
			}
		}
		else {
			// speed up pods :)
			collection.resize( startIndex + size );
			fread( &collection[startIndex], sizeof( Value ), size, reader.handle );
		}
	}

	template< typename Value >
	void write( TextWriter &writer, const std::vector<Value> &collection ) {
		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			put( writer, *it );
		}
	}

	template< typename Value >
	void read( TextReader &reader, std::vector<Value> &collection ) {
		int size = (int) reader.mapNode->size();
		collection.reserve( collection.size() + size );

		for( int i = 0 ; i < size ; ++i ) {
			Value value;
			get( reader, value );
			collection.push_back( std::move( value ) );
		}
	}

	// std::list
	template< typename Value >
	void write( BinaryWriter &writer, const std::list<Value> &collection ) {
		unsigned int size = (unsigned int) collection.size();
		write( writer, size );

		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			write( writer, *it );
		}
	}

	template< typename Value >
	void read( BinaryReader &reader, std::list<Value> &collection ) {
		unsigned int size;
		read( reader, size );

		unsigned int startIndex = (unsigned int) collection.size();

		for( unsigned int i = 0 ; i < size ; ++i ) {
			Value value;
			read( reader, value );
			collection.push_back( std::move( value ) );
		}
	}

	template< typename Value >
	void write( TextWriter &writer, const std::list<Value> &collection ) {
		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			put( writer, *it );
		}
	}

	template< typename Value >
	void read( TextReader &reader, std::list<Value> &collection ) {
		int size = (int) reader.mapNode->size();
		
		for( int i = 0 ; i < size ; ++i ) {
			Value value;
			get( reader, value );
			collection.push_back( std::move( value ) );
		}
	}

	// std::string
	void read( BinaryReader &reader, std::string &value ) {
		unsigned int size;
		read( reader, size );
		value.resize( size );
		fread( &value[0], size, 1, reader.handle );
	}

	void write( BinaryWriter &writer, const std::string &value ) {
		unsigned int size = (unsigned int)  value.size();
		fwrite( &size, sizeof( unsigned int ), 1, writer.handle );
		fwrite( &value[0], size, 1, writer.handle );
	}

	void read( TextReader &reader, std::string &value ) {
		value = reader.mapNode->data().content;
	}

	void write( TextWriter &writer, const std::string &value ) {
		writer.mapNode->push_back( value );
	}

	// std::pair
	template< typename First, typename Second >
	void write( BinaryWriter &writer, const std::pair< First, Second > &pair ) {
		put( writer, pair.first );
		put( writer, pair.second );
	}

	template< typename First, typename Second >
	void read( BinaryReader &reader, std::pair< First, Second > &pair ) {
		get( reader, pair.first );
		get( reader, pair.second );
	}

	template< typename First, typename Second >
	void write( TextWriter &writer, const std::pair< First, Second > &pair ) {
		if( !detail::is_simple< First >::value || !writer.keyNode ) {
			put( writer, pair.first );
			put( writer, pair.second );
		}
		else {
			putAsKey( writer, "", pair.first );

			write( writer, pair.second );
		}
	}

	template< typename First, typename Second >
	void read( TextReader &reader, std::pair< First, Second > &pair ) {
		if( !detail::is_simple< First >::value || !reader.keyNode ) {
			get( reader, pair.first );
			get( reader, pair.second );
		}
		else {
			getAsKey( reader, "", pair.first );

			read( reader, pair.second );
		}
	}
	
	// std::map
	template< typename Key, typename Value >
	void write( BinaryWriter &writer, const std::map< Key, Value > &collection ) {
		unsigned int size = (unsigned int) collection.size();
		write( writer, size );
		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			write( writer, *it );
		}
	}

	template< typename Key,typename Value >
	void read( BinaryReader &reader, std::map< Key, Value > &collection ) {
		unsigned int size;
		read( reader, size );

		for( unsigned int i = 0 ; i < size ; ++i ) {
			std::pair< Key, Value > pair;
			read( reader, pair );
			collection.insert( std::move( pair ) );
		}
	}

	template< typename Key, typename Value >
	void write( TextWriter &writer, const std::map< Key, Value > &collection ) {
		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			put( writer, *it );
		}
	}

	template< typename Key, typename Value >
	void read( TextReader &reader, std::map< Key, Value > &collection ) {
		int size = (int) reader.mapNode->size();

		for( int i = 0 ; i < size ; ++i ) {
			std::pair< Key, Value > pair;
			get( reader, pair );
			collection.insert( std::move( pair ) );
		}
	}
}