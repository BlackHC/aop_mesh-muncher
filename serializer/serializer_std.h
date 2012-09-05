#pragma once

#include "serializer.h"

#include <vector>
#include <map>
#include <string>

#include <boost/type_traits/is_fundamental.hpp>

namespace Serializer {
	// std::vector
	template< typename Value >
	void write( BinaryWriter &writer, const std::vector<Value> &collection ) {
		unsigned int size = (unsigned int) collection.size();
		write( writer, size );

		if( !boost::is_fundamental< Value >::value && !RawMode< Value >::value ) {
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
	void write( TextWriter &writer, const std::vector<Value> &collection ) {
		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			put( writer, "", *it );
		}
	}

	template< typename Value >
	void read( BinaryReader &reader, std::vector<Value> &collection ) {
		unsigned int size;
		read( reader, size );

		unsigned int startIndex = (unsigned int) collection.size();
		collection.reserve( startIndex + size );

		if( !boost::is_fundamental< Value >::value && !RawMode< Value >::value ) {
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
	void read( TextReader &reader, std::vector<Value> &collection ) {
		unsigned int size = (unsigned int) reader.current->size();
		collection.reserve( collection.size() + size );

		ptree *parent = reader.current;

		auto end = reader.current->end();
		for( auto it = reader.current->begin() ; it != end ; ++it ) {
			reader.current = &it->second;
			Value value;
			read( reader, value );
			collection.push_back( std::move( value ) );
		}

		reader.current = parent;
	}	

	// std::string
	void read( BinaryReader &reader, std::string &value ) {
		unsigned int size;
		read( reader, size );
		value.resize( size );
		fread( &value[0], size, 1, reader.handle );
	}

	void read( TextReader &reader, std::string &value ) {
		value = reader.current->get_value<std::string>();
	}

	void write( BinaryWriter &writer, const std::string &value ) {
		unsigned int size = (unsigned int)  value.size();
		fwrite( &size, sizeof( unsigned int ), 1, writer.handle );
		fwrite( &value[0], size, 1, writer.handle );
	}

	void write( TextWriter &writer, const std::string &value ) {
		writer.current->put_value( value );
	}

	// std::pair
	template< typename Writer, typename First, typename Second >
	void write( Writer &writer, const std::pair< First, Second > &pair ) {
		put( writer, "first", pair.first );
		put( writer, "second", pair.second );
	}

	template< typename Reader, typename First, typename Second >
	void read( Reader &reader, std::pair< First, Second > &pair ) {
		get( reader, "first", pair.first );
		get( reader, "second", pair.second );
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

	template< typename Key, typename Value >
	void write( TextWriter &writer, const std::map< Key, Value > &collection ) {
		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			put( writer, "", *it );
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
	void read( TextReader &reader, std::map< Key, Value > &collection ) {
		unsigned int size = (unsigned int) reader.current->size();

		ptree *parent = reader.current;

		auto end = reader.current->end();
		for( auto it = reader.current->begin() ; it != end ; ++it ) {
			reader.current = &it->second;
			std::pair< Key, Value > pair;
			read( reader, pair );
			collection.insert( std::move( pair ) );
		}

		reader.current = parent;
	}

}