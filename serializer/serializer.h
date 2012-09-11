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
	namespace detail {
		// check whether X::serializer_use_raw_mode exists
		template< typename X >
		struct has_serializer_use_raw_mode {
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
	}

	template< typename X >
	struct RawMode {
		static const bool value = detail::has_serializer_use_raw_mode<X>::value;
	};

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

	namespace detail {
		// shared base class by TextWriter and TextReader
		struct TextBase : boost::noncopyable {
			wml::Node root;

			wml::Node *mapNode;

			// there are cases when we have to use a dummy key '-', eg when serializing arrays or std::pairs
			// if keyNode is set, putAsKey and getAsKey will write to it instead of creating a sub key in mapNode
			wml::Node *keyNode;

			TextBase() : mapNode( &root ), keyNode( nullptr ) {}

			// this class can be used to create a new environment for processing a certain node
			template< class Base >
			struct Environment {
				Base &base;

				wml::Node *mapNode;
				wml::Node *keyNode;

				Environment( Base &base, wml::Node *newMapNode, wml::Node *newKeyNode = nullptr ) 
					: base( base ),
					mapNode( base.mapNode ),
					keyNode( base.keyNode )
				{
					base.mapNode = newMapNode;
					base.keyNode = newKeyNode;
				}

				~Environment() {
					base.mapNode = mapNode;
					base.keyNode = keyNode;
				}
			};
		};
	}

	struct TextWriter : detail::TextBase {
		std::string filename;

		TextWriter( const char *filename ) : filename( filename ) {}
		~TextWriter() {
			wml::emitFile( filename, root );
		}

		typedef detail::TextBase::Environment< TextWriter > Environment;
	};

	struct TextReader : detail::TextBase {
		// node counter for unnamed gets (so we can iterate over all sub nodes of mapNode)
		int unnamedCounter;

		TextReader( const char *filename ) : unnamedCounter( 0 ) {
			root = wml::parseFile( filename );
		}

		struct Environment : detail::TextBase::Environment< TextReader > {
			typedef detail::TextBase::Environment< TextReader > super;
			int unnamedCounter;

			Environment( TextReader &reader, wml::Node *newMapNode, wml::Node *newKeyNode = nullptr )
				: super( reader, newMapNode, newKeyNode ), unnamedCounter( reader.unnamedCounter )
			{
				reader.unnamedCounter = 0;
			}

			~Environment() {
				base.unnamedCounter = unnamedCounter;
			}
		};
	};
	
	//////////////////////////////////////////////////////////////////////////
	// binary mode passthrough
	
	template< typename Value >
	void put( BinaryWriter &writer, const Value &value ) {
		write( writer, value );
	}

	template< typename Value >
	void put( BinaryWriter &writer, const char *key, const Value &value ) {
		write( writer, value );
	}
		
	template< typename Value >
	void putAsKey( BinaryWriter &writer, const char *key, const Value &value ) {
		write( writer, value );
	}
	
	template< typename Value >
	void get( BinaryReader &reader, Value &value ) {
		read( reader, value );
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

	//////////////////////////////////////////////////////////////////////////
	// text mode

	// invariants:
	//  put creates the key node, get reads the key node - both sets mapNode accordingly to it
	//  write pushes a value into mapNode (ie a value subnode), reads from data()

	template< typename Value >
	void put( TextWriter &writer, const char *key, const Value &value ) {
		TextWriter::Environment environment( writer, &writer.mapNode->push_back( key ) );
		write( writer, value );
	}

	// either adds value using key as key or, if possible, sets value as primary key of the whole object
	template< typename Value >
	void putAsKey( TextWriter &writer, const char *key, const Value &value ) {
		if( !writer.keyNode ) {
			put( writer, key, value );
		}
		else {
			{
				TextWriter::Environment environment( writer, writer.keyNode );

				write( writer, value );				
			}
			
			// keyNode has been used - additional putAsKey will be simple puts
			writer.keyNode = nullptr;
		}
	}

	namespace detail {
		// this is better than what is around on the internet
		// see http://stackoverflow.com/questions/2733377/is-there-a-way-to-test-whether-a-c-class-has-a-default-constructor-other-than
		template< class T >
		class is_default_constructible {
			typedef int yes;
			typedef char no;
		
			// for fun: the other version, does not work... wtf?
#if 1
			template<int x, int y> class is_equal {};
			template<int x> class is_equal<x,x> { typedef void type; };

			template< class U >
			static yes sfinae( typename is_equal< sizeof U(), sizeof U() >::type * );
#else
			template<int x> class is_okay { typedef void type; };
			
			template< class U >
			static yes sfinae( typename is_okay< sizeof U() >::type * );
#endif

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

			TextReader::Environment environment( reader, &*it );
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
	
	// if we don't pass a default value, we either construct a default value, if possible
	template< typename Value >
	typename boost::enable_if< detail::is_default_constructible< Value > >::type 
	get( TextReader &reader, const char *key, Value &value ) {
		if( !detail::tryGet( reader, key, value ) ) {
			value = Value();
		}
	}

	// or we fail, if there is no default constructor
	template< typename Value >
	typename boost::enable_if_c< !detail::is_default_constructible< Value >::value >::type 
	get( TextReader &reader, const char *key, Value &value ) {
			if( !detail::tryGet( reader, key, value ) ) {
				reader.mapNode->error( boost::str( boost::format( "'%s' not found!") % key ) );
			}
	}

	// get value from the object name, if possible, or from a key-value pair
	template< typename Value >
	void getAsKey( TextReader &reader, const char *key, Value &value ) {
		if( !reader.keyNode ) {
			get( reader, key, value );
		}
		else {
			{
				TextReader::Environment environment( reader, reader.keyNode );
			
				read( reader, value );
			}
			
			// keyNode has been used - additional getAsKey will be gets
			reader.keyNode = nullptr;
		}
	}

	// unnamed put/get
	// the IsSimple type trait is for types that want to use only one read/write call instead of gets/puts to look like a fundamental type
	template<typename Value>
	struct IsSimple {
		static const bool value = false;
	};

	namespace detail {
		// helper trait to determine whether a type is 'simple'
		// ie whether it can be written as one value (and no sub keys)
		template< typename Value >
		struct is_simple_wo_cv {
			static const bool value = boost::is_fundamental< Value >::value || RawMode< Value >::value || IsSimple< Value >::value;
		};

		template<>
		struct is_simple_wo_cv< std::string > {
			static const bool value = true;
		};

		template< typename Value >
		struct is_simple {
			static const bool value = is_simple_wo_cv< typename boost::remove_cv< Value >::type >::value;
		};

		// helper trait to determine whether we can Value (in binary mode)
		template< typename Value >
		struct can_be_dumped_wo_cv {
			static const bool value = boost::is_fundamental< Value >::value || RawMode< Value >::value;
		};

		template< typename Value >
		struct can_be_dumped {
			static const bool value = can_be_dumped_wo_cv< typename boost::remove_cv< Value >::type >::value;
		};
	}

	template< typename Value >
	void put( TextWriter &writer, const Value &value ) {
		wml::Node keyNode;

		TextWriter::Environment environment( writer, &writer.mapNode->push_back( "-" ), &keyNode );

		write( writer, value );

		// is the key a simple value?
		if( detail::is_simple< Value >::value ) {
			// move the content up
			writer.mapNode->content = std::move( writer.mapNode->data().content );
			writer.mapNode->nodes.clear();
		}
		else 
		// has the keyNode been 'used'?
		if( !writer.keyNode ) {		
			// verify there is only one data node
			BOOST_ASSERT( keyNode.size() == 1 );

			// move the contents into mapNode
			writer.mapNode->content = std::move( keyNode.data().content );
		}

	}

	template< typename Value >
	void get( TextReader &reader, Value &value ) {
		wml::Node &itemNode = reader.mapNode->nodes[ reader.unnamedCounter++ ];

		// TODO: on demand keyNode creation would be better
		wml::Node keyNode;
		// we move the content, because it is either meaningful or useless
		keyNode.push_back( std::move( itemNode.content ) );
		keyNode.context = itemNode.context;

		if( detail::is_simple< Value >::value ) {
			TextReader::Environment environment( reader, &keyNode );
			read( reader, value );
		}
		else {
			TextReader::Environment environment( reader, &itemNode, &keyNode );
			read( reader, value );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// read and write methods

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
	write( BinaryWriter &writer, const Value &value ) {
		fwrite( &value, sizeof( Value ), 1, writer.handle );
	}

	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
	read( TextReader &reader, Value &value ) {
		// TODO: add checks and error to data() if there is none [9/9/2012 Andreas]
		value = reader.mapNode->data().as<Value>();
	}

	template< typename Value >
	typename boost::enable_if< boost::is_arithmetic< Value > >::type
	write( TextWriter &writer, const Value &value ) {
		writer.mapNode->push_back( value );
	}

	//////////////////////////////////////////////////////////////////////////
	// enums
	
	template< typename Value >
	typename boost::enable_if< boost::is_enum< Value > >::type
		read( BinaryReader &reader, Value &value ) {
			fread( &value, sizeof( Value ), 1, reader.handle );
	}

	template< typename Value >
	typename boost::enable_if< boost::is_enum< Value > >::type
		write( BinaryWriter &writer, const Value &value ) {
			fwrite( &value, sizeof( Value ), 1, writer.handle );
	}

	template< typename Value >
	struct Reflection {
		/*
		static std::pair< const char *, Value > get( int index ) {
			return std::make_pair( nullptr, Value() ); 
		}
		}*/
	};

	namespace detail {
		template< typename X >
		struct has_reflection {
			// Types "yes" and "no" are guaranteed to have different sizes,
			// specifically sizeof(yes) == 1 and sizeof(no) == 2.
			typedef char yes[1];
			typedef char no[2];

			template <typename C>
			static yes& test( decltype( C::get(0), void() ) *);

			template <typename>
			static no& test(...);

			// If the "sizeof" the result of calling test<T>(0) would be equal to the sizeof(yes),
			// the first overload worked and T has a nested type named foobar.
			static const bool value = sizeof(test<Serializer::Reflection<X>>(0)) == sizeof(yes);
		};
	};

#define _SERIALIZER_STD_REFLECTION_PAIR( r, data, labelValueSeq ) \
	if( !index-- ) { \
		return std::make_pair labelValueSeq ; \
	}

#define SERIALIZER_REFLECTION( Value, labelValueSeqSeq ) \
	template<> \
	struct Serializer::Reflection<Value> { \
		static std::pair< const char *, Value > get( int index ) { \
			BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_REFLECTION_PAIR, BOOST_PP_NIL, labelValueSeqSeq ) \
			return std::make_pair( nullptr, Value() ); \
		} \
	};
	
	// using serializer_reflection
	template< typename Value >
	typename boost::enable_if< detail::has_reflection< Value > >::type
	read( TextReader &reader, Value &value ) {
		const std::string &label = reader.mapNode->data().content;

		for( int index = 0 ; ; ++index ) {
			std::pair< const char *, Value > labelValuePair = Reflection<Value>::get( index );

			if( !labelValuePair.first ) {
				break;
			}

			if( label == labelValuePair.first ) {
				value = labelValuePair.second;
				return;
			}
		}

		// read it with lexical cast
		value = (Value) reader.mapNode->data().as<int>();
	}

	template< typename Value >
	typename boost::enable_if< detail::has_reflection< Value > >::type
	write( TextWriter &writer, const Value &value ) {
		for( int index = 0 ; ; ++index ) {
			std::pair< const char *, Value > labelValuePair = Reflection<Value>::get( index );

			if( !labelValuePair.first ) {
				break;
			}

			if( value == labelValuePair.second ) {
				writer.mapNode->push_back( labelValuePair.first );
				return;
			}
		}

		// write as is
		writer.mapNode->push_back( (int) value );
	}

	template< typename Value >
	typename boost::enable_if_c< !detail::has_reflection< Value >::value && boost::is_enum< Value >::value >::type
	read( TextReader &reader, Value &value ) {
		int raw;
		read( reader, raw );
		value = (Value) raw;
	}

	template< typename Value >
	typename boost::enable_if_c< !detail::has_reflection< Value >::value && boost::is_enum< Value >::value >::type
	write( TextWriter &writer, const Value &value ) {
		write( writer, (int) value );
	}

	// if there is only one enum of a type in an object, you can use put/getGlobalEnum
	template< typename Value >
	typename boost::enable_if< detail::has_reflection< typename boost::remove_cv< Value >::type > >::type
	putGlobalEnum( BinaryWriter &writer, const Value &value ) {
		write( writer, value );
	}

	template< typename Value >
	typename boost::enable_if< detail::has_reflection< typename boost::remove_cv< Value >::type > >::type
	getGlobalEnum( BinaryReader &reader, Value &value ) {
		read( reader, value );
	}

	template< typename Value >
	typename boost::enable_if< detail::has_reflection< typename boost::remove_cv< Value >::type > >::type
	putGlobalEnum( TextWriter &writer, const Value &value ) {
		write( writer, value );
	}

	template< typename Value >
	typename boost::enable_if< detail::has_reflection< typename boost::remove_cv< Value >::type > >::type
	getGlobalEnum( TextReader &reader, Value &value ) {
		for( int index = 0 ; ; ++index ) {
			std::pair< const char *, Value > labelValuePair = Reflection<Value>::get( index );

			if( !labelValuePair.first ) {
				break;
			}

			auto it = reader.mapNode->find( labelValuePair.first );

			if( it != reader.mapNode->not_found() ) {
				value = labelValuePair.second;
				return;
			}
		}

		// error otherwise
		{
			std::string values;
			for( int index = 0 ; ; ++index ) {
				std::pair< const char *, Value > labelValuePair = Reflection<Value>::get( index );

				if( !labelValuePair.first ) {
					break;
				}

				if( index ) {
					values.push_back( ' ' );
				}

				values.append( labelValuePair.first );
			}

			reader.mapNode->error( boost::str( boost::format( "expected global enum with possible values: %s") % values ) );
		}
	}

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

#define SERIALIZER_ENABLE_RAW_MODE() \
	typedef void serializer_use_raw_mode;

#define SERIALIZER_ENABLE_RAW_MODE_EXTERN( type ) \
	template<> \
	struct ::Serializer::RawMode< type > { \
		const static bool value = true; \
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type read( BinaryReader &reader, X &value ) {
		fread( &value, sizeof( X ), 1, reader.handle );
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type write( BinaryWriter &writer, const X &value ) {
		fwrite( &value, sizeof( X ), 1, writer.handle );
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type read( TextReader &reader, X &value ) {
#ifdef SERIALIZER_TEXT_ALLOW_RAW_DATA
		const std::string &data = reader.mapNode->data().content;
		value = *reinterpret_cast<const X*>( &data.front() );
#else
		reader.mapNode->error( "raw data not allowed! #define SERIALIZER_TEXT_ALLOW_RAW_DATA to allow!")
#endif
	}

	template< typename X >
	typename boost::enable_if< RawMode< X > >::type write( TextWriter &writer, const X &value ) {
#ifdef SERIALIZER_TEXT_ALLOW_RAW_DATA
		writer.mapNode->push_back( std::string( (const char*) &value, (const char*) &value + sizeof( X ) ) );
#endif
	}

	// member helpers
#define SERIALIZER_GET_VARIABLE( reader, variable ) \
	Serializer::get( reader, #variable, variable )

#define SERIALIZER_PUT_VARIABLE( writer, variable ) \
	Serializer::put( writer, #variable, variable )

#define SERIALIZER_GET_KEY( reader, variable ) \
	Serializer::getAsKey( reader, #variable, variable )

#define SERIALIZER_PUT_KEY( writer, variable ) \
	Serializer::putAsKey( writer, #variable, variable )

#define SERIALIZER_GET_FIELD( reader, object, field ) \
	Serializer::get( reader, #field, (object).field )

#define SERIALIZER_PUT_FIELD( writer, object, field ) \
	Serializer::put( writer, #field, (object).field )

	// standard helpers
#define SERIALIZER_PAIR_IMPL( firstVar, secondVar ) \
	template< typename Reader > \
	void serializer_read( Reader &reader ) { \
		std::pair< decltype( firstVar ), decltype( secondVar ) > pair; \
		Serializer::read( reader, pair ); \
		firstVar = std::move( pair.first ); \
		secondVar = std::move( pair.second ); \
	} \
	template< typename Writer > \
	void serializer_write( Writer &writer ) const { \
		Serializer::write( writer, std::make_pair( firstVar, secondVar ) ); \
	}

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

	// TODO: rename to SERIALIZER_FIRST_AS_KEY_IMPL
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

#define _SERIALIZER_STD_GET_GLOBAL( r, data, field ) Serializer::getGlobalEnum( reader, field );
#define _SERIALIZER_STD_PUT_GLOBAL( r, data, field ) Serializer::putGlobalEnum( writer, field );
#define SERIALIZER_IMPL( primaryKey, fieldSeq, globalEnumSeq ) \
	template< typename Reader > \
	void serializer_read( Reader &reader ) { \
		Serializer::getAsKey( reader, BOOST_PP_STRINGIZE( primaryKey ), primaryKey ); \
		BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_GET, BOOST_PP_NIL, fieldSeq ) \
		BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_GET_GLOBAL, BOOST_PP_NIL, globalEnumSeq ) \
	} \
	template< typename Writer > \
	void serializer_write( Writer &writer ) const { \
		Serializer::putAsKey( writer, BOOST_PP_STRINGIZE( primaryKey ), primaryKey ); \
		BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_PUT, BOOST_PP_NIL, fieldSeq ) \
		BOOST_PP_SEQ_FOR_EACH( _SERIALIZER_STD_PUT_GLOBAL, BOOST_PP_NIL, globalEnumSeq ) \
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