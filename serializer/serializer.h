#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

// for helper macros
#include <boost/preprocessor/seq/for_each.hpp>

//#define SERIALIZER_SUPPORT_STL
//#define SERIALIZER_SUPPORT_EIGEN

//#define SERIALIZER_TEXT_ALLOW_RAW_DATA

/* custom type serialization I

namespace Serializer {
template< typename Reader >
void read( Reader &reader, X &value ) {
}

template< typename Writer >
void write( Writer &writer, const X &value ) {
}
*/

/* custom type serialization II

class ... {
	template< typename Reader >
		void serializer_read( Reader &reader ) {
	}

	template< typename Writer >
	void serializer_write( Writer &writer ) const {
	}
}
*/

/*template< typename Reader, typename Scalar, int N >
void read( Reader &reader, Eigen::Matrix< Scalar, N, 1 > &value ) {
	read( reader, static_cast<Scalar[N]>(&value[0]) );
}

template< typename Writer, typename Scalar, int N >
void write( Writer &writer, const Eigen::Matrix<Scalar, N, 1> &value ) {
	write( writer, static_cast<const Scalar[N]>(&value[0]) );
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
	struct BinaryWriter : boost::noncopyable {
		FILE *handle;

		BinaryWriter( const char *filename ) : handle( fopen( filename , "wb" ) ) {}
		~BinaryWriter() {
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

	struct TextWriter : boost::noncopyable {
		ptree root, *current;

		std::string filename;

		TextWriter( const char *filename ) : filename( filename ), current( &root ) {}
		~TextWriter() {
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
	void put( BinaryWriter &writer, const char *key, const Value &value ) {
		write( writer, value );
	}

	template< typename Value >
	void put( TextWriter &writer, const char *key, const Value &value ) {
		ptree *parent = writer.current;
		writer.current = &parent->push_back( std::make_pair( key, ptree() ) )->second;
		write( writer, value );
		writer.current = parent;
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

	// use class methods if they exist
	template< typename Reader, typename X >
	auto read( Reader &reader, X &value ) -> decltype( value.serializer_read( reader ), void() ) {
		value.serializer_read( reader );
	}

	template< typename Writer, typename X >
	auto write( Writer &writer, const X &value ) -> decltype( value.serializer_write( writer ), void() ) {
		value.serializer_write( writer );
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
		write( BinaryWriter &writer, const Value &value ) {
			fwrite( &value, sizeof( Value ), 1, writer.handle );
	}

	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
		write( TextWriter &writer, const Value &value ) {
			writer.current->put_value( value );
	}

	// enums
	/*template<typename E>
	struct EnumSimpleReflection {
		//static const char* labels[0];
	};*/

	template<typename E>
	struct EnumReflection {
		//static const std::pair<const char*, E> labelValuePairs[0];
	};

	template< typename Value >
	typename boost::enable_if< boost::is_enum< Value > >::type
		read( BinaryReader &reader, Value &value ) {
			fread( &value, sizeof( Value ), 1, reader.handle );
	}

	// using labelValuePairs
	template< typename Value >
	typename boost::enable_if< boost::is_enum< Value > /*&& sizeof( EnumReflection<Value>::labels )*/  >::type
		read( TextReader &reader, Value &value ) {
		std::string label;
		label = reader.current->get_value<std::string>();

		for( int i = 0 ; i < boost::size( EnumReflection<Value>::labelValuePairs ) ; ++i ) {
			if( label == EnumReflection<Value>::labelValuePairs[i].label ) {
				value = EnumReflection<Value>::labelValuePairs[i].value;
				return;
			}
		}

		// try it as int
		value = (Value) reader.current->get_value<int>();
	}

	// using labels
	/*template< typename Value >
	typename boost::enable_if_c< boost::is_enum< Value >::value && sizeof( EnumSimpleReflection<Value>::labels ) >::type
		read( TextReader &reader, Value &value ) {
		std::string label;
		label = reader.current->get_value<std::string>();

		for( int i = 0 ; i < boost::size( EnumSimpleReflection<Value>::labels ) ; ++i ) {
			if( label == EnumSimpleReflection<Value>::labels[i] ) {
				value = (E) i;
				return;
			}
		}

		// try it as int
		value = (E) reader.current->get_value<int>();
	}*/

	template< typename Value >
	typename boost::enable_if< boost::is_enum< Value > >::type
		write( BinaryWriter &writer, const Value &value ) {
			fwrite( &value, sizeof( Value ), 1, writer.handle );
	}

	template< typename Value >
	typename boost::enable_if< boost::is_enum< Value > /* && sizeof( EnumReflection<Value>::labels ) */>::type
		write( TextWriter &writer, const Value &value ) {
		for( int i = 0 ; i < boost::size( EnumReflection<Value>::labelValuePairs ) ; ++i ) {
			if( value == EnumReflection<Value>::labelValuePairs[i].value ) {
				writer.current->put_value( EnumReflection<Value>::labelValuePairs[i].label );
				return;
			}
		}
		writer.current->put_value<int>( value );
	}

	/*template< typename Value >
	typename boost::enable_if_c< boost::is_enum< Value >::value && sizeof( EnumSimpleReflection<Value>::labels[0] ) != 0 >::type
		write( TextWriter &writer, const Value &value ) {
		if( value >= 0 && value < boost::size( EnumSimpleReflection<Value>::labels ) ) {
			writer.current->put_value( EnumSimpleReflection<Value>::labels[value] );
		}
		else {
			writer.current->put_value<int>( value );
		}
	}*/

	// static array
	template< typename Value, int N >
	void write( BinaryWriter &writer, const Value (&array)[N] ) {
		for( int i = 0 ; i < N ; ++i ) {
			write( writer, array[i] );
		}
	}

	template< typename Value, int N >
	void write( TextWriter &writer, const Value (&array)[N] ) {
		for( int i = 0 ; i < N ; ++i ) {
			put( writer, "", array[i] );
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

	// raw serialization
	template< typename X >
	struct RawMode {
		enum Value {
			value = false
		};
	};

#define SERIALIZER_ENABLE_RAW_MODE( type ) \
	template<> \
	struct ::Serializer::RawMode< type > { \
		enum Value { \
			value = true \
		}; \
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type read( TextReader &reader, X &value ) {
#ifdef SERIALIZER_TEXT_ALLOW_RAW_DATA
		std::string data = reader.current->get_value<std::string>();
		value = *reinterpret_cast<X*>( &data.front() );
#else
		throw std::invalid_argument( "raw data not allowed!" );
#endif
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type read( BinaryReader &reader, X &value ) {
		fread( &value, sizeof( X ), 1, reader.handle );
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type write( TextWriter &writer, const X &value ) {
#ifdef SERIALIZER_TEXT_ALLOW_RAW_DATA
		writer.current->put_value( std::string( (const char*) &value, (const char*) &value + sizeof( X ) ) );
#endif
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type write( BinaryWriter &writer, const X &value ) {
		fwrite( &value, sizeof( X ), 1, writer.handle );
	}

	// member helpers
#define SERIALIZER_GET_VARIABLE( reader, field ) \
	Serializer::get( reader, #field, field )

#define SERIALIZER_PUT_VARIABLE( writer, field ) \
	Serializer::put( writer, #field, field )

#define SERIALIZER_GET_FIELD( reader, object, field ) \
	Serializer::get( reader, #field, (object).field )

#define SERIALIZER_PUT_FIELD( writer, object, field ) \
	Serializer::put( writer, #field, (object).field )

	// standard helpers
#define _SERIALIZER_STD_GET( r, data, field ) Serializer::get( reader, BOOST_PP_STRINGIZE( field ), field );
#define _SERIALIZER_STD_PUT( r, data, field ) Serializer::put( writer, BOOST_PP_STRINGIZE( field ), field );
#define SERIALIZER_DEFAULT_IMPL( fieldSeq ) \
	template< typename Reader > \
	void serializer_read( Reader &reader ) { \
		BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_GET, BOOST_PP_NIL, fieldSeq ) \
	} \
	template< typename Writer > \
	void serializer_write( Writer &writer ) const { \
		BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_PUT, BOOST_PP_NIL, fieldSeq ) \
	}

#define _SERIALIZER_STD_EXTERN_GET( r, data, field )  Serializer::get( reader, BOOST_PP_STRINGIZE( field ), value. field );
#define _SERIALIZER_STD_EXTERN_PUT( r, data, field ) Serializer::put( writer, BOOST_PP_STRINGIZE( field ), value. field );
#define SERIALIZER_DEFAULT_EXTERN_IMPL( type, fieldSeq ) \
	namespace Serializer { \
		template< typename Reader > \
		void read( Reader &reader, type &value ) { \
			BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_EXTERN_GET, BOOST_PP_NIL, fieldSeq ) \
		} \
		template< typename Writer > \
		void write( Writer &writer, const type &value ) { \
			BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_EXTERN_PUT, BOOST_PP_NIL, fieldSeq ) \
		} \
	}
}

#ifdef SERIALIZER_SUPPORT_STL
#	include "serializer_std.h"
#endif
#ifdef SERIALIZER_SUPPORT_EIGEN
#	include "serializer_eigen.h"
#endif