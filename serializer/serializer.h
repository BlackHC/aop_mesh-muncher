#pragma once


#include <wml.h>
#include <stdio.h>
#include <boost/noncopyable.hpp>
#include <exception>

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
		std::string filename;

		wml::Node root;

		wml::Node *mapNode;
		wml::Node *writeNode;
	
		bool firstKeyAllowed;

		TextWriter( const char *filename ) : filename( filename ), mapNode( &root ), writeNode( mapNode ), firstKeyAllowed( true ) {}
		~TextWriter() {
			wml::emitFile( filename, root );
		}

		struct Scope {
			TextWriter &writer;
			wml::Node *mapNode;
			bool firstKeyAllowed;

			Scope( TextWriter &writer, wml::Node *newMapNode, bool newFirstKeyAllowed = false ) 
				: writer( writer ),
					mapNode( writer.mapNode ),
					firstKeyAllowed( writer.firstKeyAllowed )
			{
				writer.mapNode = newMapNode;
				writer.firstKeyAllowed = newFirstKeyAllowed;
			}
			~Scope() {
				writer.mapNode = mapNode;
				writer.firstKeyAllowed = firstKeyAllowed;
			}
		};
	};

	struct TextReader : boost::noncopyable {
		wml::Node root;

		wml::Node *mapNode;
		wml::Node *readNode;

		bool firstKeyAllowed;
		int unnamedCounter;

		TextReader( const char *filename ) : mapNode( &root ), readNode( mapNode ), firstKeyAllowed( true ), unnamedCounter( 0 ) {
			root = wml::parseFile( filename );
		}

		struct Scope {
			TextReader &reader;
			wml::Node *mapNode;
			bool firstKeyAllowed;
			int unnamedCounter;

			Scope( TextReader &reader, wml::Node *newMapNode, bool newFirstKeyAllowed = false ) 
				: reader( reader ),
					mapNode( reader.mapNode ),
					firstKeyAllowed( reader.firstKeyAllowed ),
					unnamedCounter( reader.unnamedCounter ) 
			{
				reader.mapNode = newMapNode;
				reader.firstKeyAllowed = newFirstKeyAllowed;
				reader.unnamedCounter = 0;
			}
			~Scope() {
				reader.mapNode = mapNode;
				reader.firstKeyAllowed = firstKeyAllowed;
				reader.unnamedCounter = unnamedCounter;
			}
		};
	};
		
	template< typename Value >
	void put( BinaryWriter &writer, const char *key, const Value &value ) {
		write( writer, value );
	}

	// pass through in binary mode
	template< typename Value >
	void putAsKey( BinaryWriter &writer, const char *key, const Value &value ) {
		write( writer, value );
	}

	template< typename Value >
	void put( TextWriter &writer, const char *key, const Value &value ) {
		TextWriter::Scope scope( writer, &writer.mapNode->push_back( key ) );

		wml::Node writeNode;
		writer.writeNode = &writeNode;

		write( writer, value );

		if( !writeNode.empty() || !writeNode.content.empty() ) {
			writer.mapNode->push_back( std::move( writeNode ) );
		}
	}

	template< typename Value >
	void putAsKey( TextWriter &writer, const char *key, const Value &value ) {
		if( !writer.firstKeyAllowed ) {
			put( writer, key, value );
		}
		else {
			writer.firstKeyAllowed = false;

			// make sure we dont add children by accident (ie bad first key)
			int size = (int) writer.mapNode->size();

			writer.writeNode = writer.mapNode;
			write( writer, value );

			writer.firstKeyAllowed = false;

			BOOST_ASSERT( size == writer.mapNode->size() );
		}
	}
		
	template< typename Value >
	void get( BinaryReader &reader, const char *key, Value &value ) {
		read( reader, value );
	}

	template< typename Value >
	void getAsKey( BinaryReader &reader, const char *key, Value &value ) {
		read( reader, value );
	}

	// default value does not matter in the binary version
	template< typename Value >
	void get( BinaryReader &reader, const char *key, Value &value, const Value & ) {
		read( reader, value );
	}

	namespace detail {
		// this is better than what is around on the internet
		// see http://stackoverflow.com/questions/2733377/is-there-a-way-to-test-whether-a-c-class-has-a-default-constructor-other-than
		template< class T >
		class is_default_constructible {
			typedef int yes;
			typedef char no;

			template<int x>
			class receive_size{
			};

			template<>
			class receive_size<0> {
				typedef void type;
			};

			template< class U >
			static yes sfinae( typename receive_size< int( sizeof( U() ) ) - int( sizeof( U ) ) >::type * );

			template< class U >
			static no sfinae( ... );

		public:
			enum { value = sizeof( sfinae<T>(0) ) == sizeof(yes) };
		};

		template< typename Value >
		bool tryGet( TextReader &reader, const char *key, Value &value ) {
			auto it = reader.mapNode->find( key );

			if( it == reader.mapNode->not_found() ) {
				return false;
			}

			TextReader::Scope scope( reader, &*it );
			reader.readNode = &reader.mapNode->data();
			if( !reader.readNode ) {
				return false;
			}
			Serializer::read( reader, value );

			return true;
		}
	}

	template< typename Value >
	void get( TextReader &reader, const char *key, Value &value, const Value &defaultValue ) {
		if( !detail::tryGet( reader, key, value ) ) {
			value = defaultValue;
		}
	}
	
	template< typename Value >
	typename boost::enable_if_c< !detail::is_default_constructible< Value >::value >::type 
	get( TextReader &reader, const char *key, Value &value ) {
		if( !detail::tryGet( reader, key, value ) ) {
			reader.mapNode->error( boost::str( boost::format( "'%s' not found!") % key ) );
		}
	}

	template< typename Value >
	typename boost::enable_if< detail::is_default_constructible< Value > >::type 
	get( TextReader &reader, const char *key, Value &value ) {
		if( !detail::tryGet( reader, key, value ) ) {
			value = Value();
		}
	}

	template< typename Value >
	void getAsKey( TextReader &reader, const char *key, Value &value ) {
		if( !reader.firstKeyAllowed ) {
			get( reader, key, value );
		}
		else {
			reader.firstKeyAllowed = false;
			reader.readNode = reader.mapNode;
			read( reader, value );
			reader.firstKeyAllowed = false;
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
		value = reader.readNode->as<Value>();
	}

	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
	write( BinaryWriter &writer, const Value &value ) {
		fwrite( &value, sizeof( Value ), 1, writer.handle );
	}

	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
	write( TextWriter &writer, const Value &value ) {
		writer.writeNode->set( value );
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
		const std::string &label = reader.readNode->content;

		for( int i = 0 ; i < boost::size( EnumReflection<Value>::labelValuePairs ) ; ++i ) {
			if( label == EnumReflection<Value>::labelValuePairs[i].label ) {
				value = EnumReflection<Value>::labelValuePairs[i].value;
				return;
			}
		}

		// try it as int
		value = (Value) reader.readNode->as<int>();
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
				writer.writeNode->content = EnumReflection<Value>::labelValuePairs[i].label;
				return;
			}
		}
		writer.writeNode->set( (int) value );
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
	void read( BinaryReader &reader, Value (&array)[N] ) {
		for( int i = 0 ; i < N ; ++i ) {
			read( reader, array[i] );
		}
	}

	template< typename Value, int N >
	void write( TextWriter &writer, const Value (&array)[N] ) {
		for( int i = 0 ; i < N ; ++i ) {
			put( writer, array[i] );
		}
	}

	template< typename Value, int N >
	void read( TextReader &reader, Value (&array)[N] ) {
		int numChildren = (int) reader.mapNode->size();
		for( int i = 0 ; i < numChildren ; ++i ) {
			get( reader, array[i] );
		}
		if( numChildren != N ) {
			reader.mapNode->error( boost::str( boost::format( "expected %i array elements - only found %i!" ) % N % numChildren ) );
		}
	}

	// raw serialization
	template< typename X >
	struct RawMode {
		// Types "yes" and "no" are guaranteed to have different sizes,
		// specifically sizeof(yes) == 1 and sizeof(no) == 2.
		typedef char yes[1];
		typedef char no[2];

		template <typename C>
		static yes& test(typename C::serializer_use_raw_mode *);

		template <typename>
		static no& test(...);

		// If the "sizeof" the result of calling test<T>(0) would be equal to the sizeof(yes),
		// the first overload worked and T has a nested type named foobar.
		static const bool value = sizeof(test<X>(0)) == sizeof(yes);
	};

#define SERIALIZER_ENABLE_RAW_MODE() \
	typedef void serializer_use_raw_mode;

#define SERIALIZER_ENABLE_RAW_MODE_EXTERN( type ) \
	template<> \
	struct ::Serializer::RawMode< type > { \
		enum Value { \
			value = true \
		}; \
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type read( TextReader &reader, X &value ) {
#ifdef SERIALIZER_TEXT_ALLOW_RAW_DATA
		const std::string &data = reader.readNode->content;
		value = *reinterpret_cast<const X*>( &data.front() );
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
		writer.writeNode->content = std::string( (const char*) &value, (const char*) &value + sizeof( X ) );
#endif
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type write( BinaryWriter &writer, const X &value ) {
		fwrite( &value, sizeof( X ), 1, writer.handle );
	}

	// unnamed put/get
	namespace detail {
		template< typename Value >
		struct can_be_key {
			static const bool value = boost::is_fundamental< Value >::value || RawMode< Value >::value;
		};

		template<>
		struct can_be_key< std::string > {
			static const bool value = true;
		};
	}

	template< typename Value >
	void put( TextWriter &writer, Value &value ) {
		if( !writer.firstKeyAllowed || !detail::can_be_key< Value >::value ) {
			TextWriter::Scope scope( writer, &writer.mapNode->push_back( "-" ) );

			writer.firstKeyAllowed = true;
			writer.writeNode = writer.mapNode;
			write( writer, value );
		}
		else {
			putAsKey( writer, "", value );
		}
		writer.firstKeyAllowed = false;
	}

	template< typename Value >
	void get( TextReader &reader, Value &value ) {
		if( !reader.firstKeyAllowed || !detail::can_be_key< Value >::value ) {
			{
				TextReader::Scope scope( reader, &reader.mapNode->nodes[reader.unnamedCounter] );

				reader.firstKeyAllowed = true;
				reader.readNode = reader.mapNode;
				read( reader, value );
			}
			reader.unnamedCounter++;
		}
		else {
			getAsKey( reader, "", value );
		}
		reader.firstKeyAllowed = false;
	}

	// binary passthrough
	template< typename Value >
	void put( BinaryWriter &writer, const Value &value ) {
		write( writer, value );
	}

	template< typename Value >
	void get( BinaryReader &reader, Value &value ) {
		read( reader, value );
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

#define SERIALIZER_FIRST_KEY_IMPL( fieldSeq ) \
	template< typename Reader > \
	void serializer_read( Reader &reader ) { \
		Serializer::getAsKey( reader, BOOST_PP_STRINGIZE( BOOST_PP_SEQ_HEAD( fieldSeq) ), BOOST_PP_SEQ_HEAD( fieldSeq) ); \
		BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_GET, BOOST_PP_NIL, BOOST_PP_SEQ_TAIL( fieldSeq ) ) \
	} \
	template< typename Writer > \
	void serializer_write( Writer &writer ) const { \
		Serializer::putAsKey( writer, BOOST_PP_STRINGIZE( BOOST_PP_SEQ_HEAD( fieldSeq) ), BOOST_PP_SEQ_HEAD( fieldSeq) ); \
		BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_PUT, BOOST_PP_NIL, BOOST_PP_SEQ_TAIL( fieldSeq ) ) \
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