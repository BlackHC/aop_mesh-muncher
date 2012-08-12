#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

/* custom reader
namespace Serializer {
template< typename Reader >
void read( Reader &reader, X &value ) {
}

template< typename Emitter >
void write( Emitter &emitter, const X &value ) {
}
}
*/

/*template< typename Reader, typename Scalar, int N >
void read( Reader &reader, Eigen::Matrix< Scalar, N, 1 > &value ) {
	read( reader, static_cast<Scalar[N]>(&value[0]) );
}

template< typename Emitter, typename Scalar, int N >
void write( Emitter &emitter, const Eigen::Matrix<Scalar, N, 1> &value ) {
	write( emitter, static_cast<const Scalar[N]>(&value[0]) );
}*/


/*
serialize pointers using helper objects and an object id/type map.. 
OR:

using an interval set
serialize_allocated_object( T* ) adds a void *pointer and sizeof( T ) to an interval set

serialize_pointer searches for the pointer in the interval set and stores an id and relative offset
serialize_external_pointer replaces a pointer with a fixed id that has been set before (otherwise it wont work)

this will work for binary storage but makes it difficult for a human to change anything...
*/
/*
struct ptree_serializer {
	template<typename T>
	void exchange( const char *key, T &data, const T &defaultValue = T());
};
*/

// static dispatch
/*
template< typename Target >
struct ptree_static_dispatch {
	struct StaticTag {};

	template< typename Vistor, typename Param, typename Result >
	static void static_visit( const Param &p ) {
		Vistor::static_visit( p, StaticTag );
	}
};

struct ptree_static_construction {

};*/

namespace Serializer {
	typedef boost::property_tree::ptree ptree;
	struct BinaryEmitter : boost::noncopyable {
		FILE *handle;

		BinaryEmitter( const char *filename ) : handle( fopen( filename , "wb" ) ) {}
		~BinaryEmitter() {
			if( handle ) {
				fclose( handle );
			}
		}
	};

	struct BinaryReader : boost::noncopyable {
		FILE *handle;

		BinaryReader( const char *filename ) : handle( fopen( filename , "rb" ) ) {}
		~BinaryReader() {
			if( handle ) {
				fclose( handle );
			}
		}
	};

	struct TextEmitter : boost::noncopyable {
		ptree root, *current;

		std::string filename;

		TextEmitter( const char *filename ) : filename( filename ), current( &root ) {}
		~TextEmitter() {
			boost::property_tree::json_parser::write_json( filename, root );
		}
	};

	struct TextReader : boost::noncopyable {
		ptree root, *current;		

		TextReader( const char *filename ) : current( &root ) {
			boost::property_tree::json_parser::read_json( filename, root );
		}
	};

	template< typename Value >
	void put( BinaryEmitter &emitter, const char *key, const Value &value ) {
		write( emitter, value );
	}

	template< typename Value >
	void put( TextEmitter &emitter, const char *key, const Value &value ) {
		ptree *parent = emitter.current;
		emitter.current = &parent->push_back( std::make_pair( key, ptree() ) )->second;
		write( emitter, value );
		emitter.current = parent;
	}

	template< typename Value >
	void get( BinaryReader &reader, const char *key, Value &value ) {
		read( reader, value );
	}

	template< typename Value >
	void get( TextReader &reader, const char *key, Value &value, const Value &defaultValue = Value() ) {
		auto it = reader.current->find( key );
		
		if( it != reader.current->not_found() ) {
			ptree *parent = reader.current;
			reader.current = &it->second;
			read( reader, value );
			reader.current = parent;
		}
		else {
			value = defaultValue;
		}
	}

	// arithmetic types
	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
		read( BinaryReader &reader, Value &value ) {
		fread( &value, sizeof( Value ), 1, reader.handle );
	}

	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
		read( TextReader &reader, Value &value ) {
		value = reader.current->get_value<Value>();
	}

	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
		write( BinaryEmitter &emitter, const Value &value ) {
			fwrite( &value, sizeof( Value ), 1, emitter.handle );
	}

	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
		write( TextEmitter &emitter, const Value &value ) {
		emitter.current->put_value( value );
	}

	// std::vector
	template< typename Value >
	void write( BinaryEmitter &emitter, const std::vector<Value> &collection ) {
		size_t size = collection.size();
		write( emitter, size );
		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			write( emitter, *it );
		}
	}

	template< typename Value >
	void write( TextEmitter &emitter, const std::vector<Value> &collection ) {
		for( auto it = collection.begin() ; it != collection.end() ; ++it ) {
			put( emitter, "", *it );
		}
	}

	template< typename Value >
	void read( BinaryReader &reader, std::vector<Value> &collection ) {
		size_t size;
		read( reader, size );
		collection.reserve( collection.size() + size );
		for( int i = 0 ; i < size ; ++i ) {
			Value value;
			read( reader, value );
			collection.push_back( std::move( value ) );
		}
	}

	template< typename Value >
	void read( TextReader &reader, std::vector<Value> &collection ) {
		size_t size = reader.current->size();
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

	// static array
	template< typename Value, int N >
	void write( BinaryEmitter &emitter, const Value (&array)[N] ) {
		for( int i = 0 ; i < N ; ++i ) {
			write( emitter, array[i] );
		}
	}

	template< typename Value, int N >
	void write( TextEmitter &emitter, const Value (&array)[N] ) {
		for( int i = 0 ; i < N ; ++i ) {
			put( emitter, "", array[i] );
		}
	}

	template< typename Value, int N >
	void read( BinaryReader &reader, Value (&array)[N] ) {
		for( int i = 0 ; i < N ; ++i ) {
			read( reader, array[i] );
		}
	}

	template< typename Value, int N >
	void read( TextReader &reader, Value (&array)[N] ) {
		ptree *parent = reader.current;

		auto end = reader.current->end();
		int i = 0;
		for( auto it = reader.current->begin() ; it != end ; ++it, ++i ) {
			reader.current = &it->second;
			read( reader, array[i] );
		}

		reader.current = parent;
	}

	// std::string
	void read( BinaryReader &reader, std::string &value ) {
		size_t size;
		read( reader, size );
		value.resize( size + 1 );
		fread( &value[0], size, 1, reader.handle );
	}

	void read( TextReader &reader, std::string &value ) {
		value = reader.current->get_value<std::string>();
	}

	void write( BinaryEmitter &emitter, const std::string &value ) {
		size_t size = value.size();
		fwrite( &value[0], size, 1, emitter.handle );
	}

	void write( TextEmitter &emitter, const std::string &value ) {
		emitter.current->put_value( value );
	}
}

