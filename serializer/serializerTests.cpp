#include "gtest.h"

#define SERIALIZER_TEXT_ALLOW_RAW_DATA
#include "serializer.h"

/*
template< typename Reader, typename Writer >
void genericTest( const char *filename ) {
	{
		Writer writer( filename );
	}

	{
		Reader reader( filename );
	}
}
*/

#define TextBinaryTestEx( test, name ) \
	TEST( name, TextSerialization ) { \
		test < Serializer::TextReader, Serializer::TextWriter >( "Text_" #name ); \
	} \
	 \
	TEST( name, BinarySerialization ) { \
		test < Serializer::BinaryReader, Serializer::BinaryWriter >( "Binary_" #name ); \
	}

#define TextBinaryTest( test ) TextBinaryTestEx( test, test )

#define TextTestEx( test, name ) \
	TEST( name, TextSerialization ) { \
		test < Serializer::TextReader, Serializer::TextWriter >( "Text_" #name ); \
	}

#define TextTest( test ) TextTestEx( test, text )

// pure tests

template< typename Reader, typename Writer >
void PrimitiveSerializations( const char *filename ) {
	{
		int i = 10;
		float f = 3.1415;
		char c = 'a';

		Writer writer( filename );
		SERIALIZER_PUT_VARIABLE( writer, i );
		SERIALIZER_PUT_VARIABLE( writer, f );
		SERIALIZER_PUT_VARIABLE( writer, c );
	}

	{
		int i;
		float f;
		char c;

		Reader reader( filename );
		SERIALIZER_GET_VARIABLE( reader, i );
		SERIALIZER_GET_VARIABLE( reader, f );
		SERIALIZER_GET_VARIABLE( reader, c );

		EXPECT_EQ( 10, i );
		EXPECT_FLOAT_EQ( 3.1415, f );
		EXPECT_EQ( 'a', c );
	}
}

TextBinaryTest( PrimitiveSerializations );

template< typename Reader, typename Writer >
void DefaultValueSerialization( const char *filename ) {
	{
		Writer writer( filename );
	}

	{
		Reader reader( filename );

		int i = 10;
		float f = 3.1415;
		char c = 'a';

		SERIALIZER_GET_VARIABLE( reader, i );
		SERIALIZER_GET_VARIABLE( reader, f );
		SERIALIZER_GET_VARIABLE( reader, c );

		EXPECT_EQ( 10, i );
		EXPECT_FLOAT_EQ( 3.1415, f );
		EXPECT_EQ( 'a', c );
	}
}

TextTest( DefaultValueSerialization );

template< typename Reader, typename Writer >
void StaticArray( const char *filename ) {
	{
		Writer writer( filename );

		int array[100];

		for( int i = 0 ; i < 100 ; i++ )
			array[i] = i;

		SERIALIZER_PUT_VARIABLE( writer, array );
	}

	{
		Reader reader( filename );

		int array[100];

		SERIALIZER_GET_VARIABLE( reader, array );

		for( int i = 0 ; i < 100 ; i++ )
			ASSERT_EQ( i, array[i] );
	}
}

TextBinaryTest( StaticArray );

// custom type with global serializer functions
struct GlobalStruct {
	int x;
	int y;
};

namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, GlobalStruct &value ) {
		get( reader, "x", value.x );
		get( reader, "y", value.y );
	}

	template< typename Writer >
	void write( Writer &writer, const GlobalStruct &value ) {
		put( writer, "x", value.x );
		put( writer, "y", value.y );
	}
}

template< typename Reader, typename Writer >
void GlobalStructPut( const char *filename ) {
	{
		Writer writer( filename );

		GlobalStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Reader reader( filename );

		GlobalStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TextBinaryTest( GlobalStructPut );

// custom type with method serialization
struct MemberStruct {
	int x;
	int y;

	template< typename Reader >
	void serializer_read( Reader &reader ) {
		SERIALIZER_GET_VARIABLE( reader, x );
		SERIALIZER_GET_VARIABLE( reader, y );
	}

	template< typename Writer >
	void serializer_write( Writer &writer ) const {
		SERIALIZER_PUT_VARIABLE( writer, x );
		SERIALIZER_PUT_VARIABLE( writer, y );
	}
};

template< typename Reader, typename Writer >
void MemberStructTest( const char *filename ) {
	{
		Writer writer( filename );

		MemberStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Reader reader( filename );

		MemberStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TextBinaryTest( MemberStructTest );

// macro test
struct MacroTestStruct {
	int x, y;

	SERIALIZER_DEFAULT_IMPL( (x)(y) )
};

template< typename Reader, typename Writer >
void MacroTest( const char *filename ) {
	{
		Writer writer( filename );

		MacroTestStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Reader reader( filename );

		MacroTestStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TextBinaryTest( MacroTest );

struct ExternMacroStruct {
	int x, y;
};

SERIALIZER_DEFAULT_EXTERN_IMPL( ExternMacroStruct, (x)(y) );

template< typename Reader, typename Writer >
void ExternMacroTest( const char *filename ) {
	{
		Writer writer( filename );

		ExternMacroStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Reader reader( filename );

		ExternMacroStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TextBinaryTest( ExternMacroTest );

// first key intern impl

struct FirstKey {
	int x;
	int y;

	SERIALIZER_FIRST_KEY_IMPL( (x)(y) );
};

template< typename Reader, typename Writer >
void MacroFirstKeyIntern( const char *filename ) {
	{
		Writer writer( filename );

		FirstKey t = {1,2};
		SERIALIZER_PUT_VARIABLE( writer, t );
	}

	{
		Reader reader( filename );

		FirstKey t;
		SERIALIZER_GET_VARIABLE( reader, t);

		EXPECT_EQ( 1, t.x );
		EXPECT_EQ( 2, t.y );
	}
}

TextBinaryTest( MacroFirstKeyIntern );

// impl test

enum EnumC {
	EC_A,
	EC_B,
	EC_C
};

SERIALIZER_REFLECTION( EnumC, (("eca", EC_A))(("ecb", EC_B))(("ecc",EC_C)) );

struct ImplMacroTestStruct {
	int x, y;
	int key;
	EnumC c;

	SERIALIZER_IMPL( key, (x)(y), (c) )
};

template< typename Reader, typename Writer >
void ImplMacroTest( const char *filename ) {
	{
		Writer writer( filename );

		ImplMacroTestStruct x = { 10, 100, 5, EC_B };
		Serializer::put( writer, "x", x );
	}

	{
		Reader reader( filename );

		ImplMacroTestStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
		EXPECT_EQ( 5, x.key );
		EXPECT_EQ( EC_B, x.c );
	}
}

TextBinaryTest( ImplMacroTest );

struct PairMacroTestStruct {
	int x, y;

	SERIALIZER_PAIR_IMPL( x, y );
};

template< typename Reader, typename Writer >
void PairMacroTest( const char *filename ) {
	{
		Writer writer( filename );

		PairMacroTestStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Reader reader( filename );

		PairMacroTestStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TextBinaryTest( PairMacroTest );


struct PairVectorMacroTestStruct {
	int x;

	std::vector<int> y;

	SERIALIZER_PAIR_IMPL( x, y );
};

template< typename Reader, typename Writer >
void PairVectorMacroTest( const char *filename ) {
	{
		Writer writer( filename );

		PairVectorMacroTestStruct x;
		x.x = 10;
		x.y.push_back( 100 );
		x.y.push_back( 1000 );
		Serializer::put( writer, x );
	}

	{
		Reader reader( filename );

		PairVectorMacroTestStruct x;
		Serializer::get( reader, x );

		EXPECT_EQ( 10, x.x );
		ASSERT_EQ( 2, x.y.size() );
		EXPECT_EQ( 100, x.y[ 0 ] );
		EXPECT_EQ( 1000, x.y[ 1 ] );
	}
}

TextBinaryTest( PairVectorMacroTest );

// enum tests
namespace Enum {
	enum EnumA {
		EA_A,
		EA_B,
		EA_C
	};

	BOOST_STATIC_ASSERT( !Serializer::detail::has_reflection< Enum::EnumA >::value );

	template< typename Reader, typename Writer >
	void Enum_Simple( const char *filename ) {
		{
			Writer writer( filename );

			EnumA a,b,c;
			a = EA_A;
			b = EA_B;
			c = EA_C;

			SERIALIZER_PUT_VARIABLE( writer, a );
			SERIALIZER_PUT_VARIABLE( writer, b );
			SERIALIZER_PUT_VARIABLE( writer, c );
		}

		{
			Reader reader( filename );

			EnumA a,b,c;
			SERIALIZER_GET_VARIABLE( reader, a );
			SERIALIZER_GET_VARIABLE( reader, b );
			SERIALIZER_GET_VARIABLE( reader, c );

			EXPECT_EQ( EA_A, a );
			EXPECT_EQ( EA_B, b );
			EXPECT_EQ( EA_C, c );
		}
	}

	TextBinaryTest( Enum_Simple );

	enum EnumB {
		EB_A,
		EB_B,
		EB_C
	};
}

template<>
struct Serializer::Reflection<Enum::EnumB> {
	static std::pair< const char *, Enum::EnumB > get( int index ) {
		if( !index-- ) {
			return std::make_pair( "eb a", Enum::EB_A );
		}
		if( !index-- ) {
			return std::make_pair( "eb b", Enum::EB_B );
		}
		if( !index-- ) {
			return std::make_pair( "eb c", Enum::EB_C );
		}
		return std::make_pair( nullptr, Enum::EnumB() );
	}
};

BOOST_STATIC_ASSERT( Serializer::detail::has_reflection< Enum::EnumB >::value );

namespace Enum {
	template< typename Reader, typename Writer >
	void Enum_Value( const char *filename ) {
		{
			Writer writer( filename );

			EnumB a,b,c;
			a = EB_A;
			b = EB_B;
			c = EB_C;

			SERIALIZER_PUT_VARIABLE( writer, a );
			SERIALIZER_PUT_VARIABLE( writer, b );
			SERIALIZER_PUT_VARIABLE( writer, c );
		}

		{
			Reader reader( filename );

			EnumB a,b,c;
			SERIALIZER_GET_VARIABLE( reader, a );
			SERIALIZER_GET_VARIABLE( reader, b );
			SERIALIZER_GET_VARIABLE( reader, c );

			EXPECT_EQ( EB_A, a );
			EXPECT_EQ( EB_B, b );
			EXPECT_EQ( EB_C, c );
		}
	}

	TextBinaryTest( Enum_Value );

	enum EnumC {
		EC_A,
		EC_B,
		EC_C
	};
}

SERIALIZER_REFLECTION( Enum::EnumC, (("eca",Enum::EC_A))(("ecb",Enum::EC_B))(("ecc",Enum::EC_C)) );

BOOST_STATIC_ASSERT( Serializer::detail::has_reflection< Enum::EnumC >::value );

namespace Enum {
	template< typename Reader, typename Writer >
	void Enum_Macro( const char *filename ) {
		{
			Writer writer( filename );

			EnumC a,b,c;
			a = EC_A;
			b = EC_B;
			c = EC_C;

			SERIALIZER_PUT_VARIABLE( writer, a );
			SERIALIZER_PUT_VARIABLE( writer, b );
			SERIALIZER_PUT_VARIABLE( writer, c );
		}

		{
			Reader reader( filename );

			EnumC a,b,c;
			SERIALIZER_GET_VARIABLE( reader, a );
			SERIALIZER_GET_VARIABLE( reader, b );
			SERIALIZER_GET_VARIABLE( reader, c );

			EXPECT_EQ( EC_A, a );
			EXPECT_EQ( EC_B, b );
			EXPECT_EQ( EC_C, c );
		}
	}

	TextBinaryTest( Enum_Macro );

	template< typename Reader, typename Writer >
	void Enum_Global( const char *filename ) {
		{
			Writer writer( filename );

			EnumC a;
			a = EC_A;

			int x = 10;

			SERIALIZER_PUT_VARIABLE( writer, x );
			Serializer::putGlobalEnum( writer, a );
		}

		{
			Reader reader( filename );

			EnumC a;
			int x;
			SERIALIZER_GET_VARIABLE( reader, x );
			Serializer::getGlobalEnum( reader, a );

			EXPECT_EQ( 10, x );
			EXPECT_EQ( EC_A, a );
		}
	}

	TextBinaryTest( Enum_Global );
}

// std tests

#include "serializer_std.h"

template< typename Reader, typename Writer >
void StdString( const char *filename  ) {
	{
		Writer writer( filename );

		std::string text = "hello world\nhallo welt";
		SERIALIZER_PUT_VARIABLE( writer, text );
	}

	{
		Reader reader( filename );

		std::string text;
		SERIALIZER_GET_VARIABLE( reader, text );

		EXPECT_EQ( "hello world\nhallo welt", text );
	}
}

TextBinaryTest( StdString );

template< typename Reader, typename Writer >
void StdVector_Fundamental( const char *filename ) {
	{
		Writer writer( filename );

		std::vector<int> seq;

		for( int i = 0 ; i < 100 ; i++ )
			seq.push_back( i );

		SERIALIZER_PUT_VARIABLE( writer, seq );
	}

	{
		Reader reader( filename );

		std::vector<int> seq;

		SERIALIZER_GET_VARIABLE( reader, seq );

		for( int i = 0 ; i < 100 ; i++ )
			ASSERT_EQ( i, seq[i] );
	}
}

TextBinaryTest( StdVector_Fundamental );

template< typename Reader, typename Writer >
void StdVector_String( const char *filename ) {
	std::vector<std::string> ref;
	{
		Writer writer( filename );

		std::vector<std::string> &seq = ref;

		for( int i = 0 ; i < 100 ; i++ )
			seq.push_back( boost::str( boost::format( "hello %s" ) % i ) );

		SERIALIZER_PUT_VARIABLE( writer, seq );
	}

	{
		Reader reader( filename );

		std::vector<std::string> seq;

		SERIALIZER_GET_VARIABLE( reader, seq );

		for( int i = 0 ; i < 100 ; i++ )
			ASSERT_EQ( ref[i], seq[i] );
	}
}

TextBinaryTest( StdVector_String );

static int StdVector_NonFundamental_readCounter = 0;
static int StdVector_NonFundamental_writeCounter = 0;

struct NonFundamental {
	int x;

	NonFundamental() {}
	NonFundamental( int x ) : x( x ) {}

	template< typename Reader >
	void serializer_read( Reader &reader ) {
		SERIALIZER_GET_VARIABLE( reader, x );
		StdVector_NonFundamental_readCounter++;
	}

	template< typename Writer >
	void serializer_write( Writer &writer ) const {
		SERIALIZER_PUT_VARIABLE( writer, x );
		StdVector_NonFundamental_writeCounter++;
	}
};

template< typename Reader, typename Writer >
void StdVector_NonFundamental( const char *filename  ) {
	StdVector_NonFundamental_readCounter = StdVector_NonFundamental_writeCounter = 0;

	{
		Writer writer( filename );

		std::vector<NonFundamental> seq;

		for( int i = 0 ; i < 100 ; i++ )
			seq.push_back( i );

		SERIALIZER_PUT_VARIABLE( writer, seq );

		ASSERT_EQ( 100, StdVector_NonFundamental_writeCounter );
		ASSERT_EQ( 0, StdVector_NonFundamental_readCounter );
	}

	{
		Reader reader( filename );

		std::vector<NonFundamental> seq;

		SERIALIZER_GET_VARIABLE( reader, seq );

		for( int i = 0 ; i < 100 ; i++ )
			ASSERT_EQ( i, seq[i].x );

		ASSERT_EQ( 100, StdVector_NonFundamental_writeCounter );
		ASSERT_EQ( 100, StdVector_NonFundamental_readCounter );
	}
}

TextBinaryTest( StdVector_NonFundamental );

template< typename Reader, typename Writer >
void StdPair( const char *filename  ) {
	{
		Writer writer( filename );

		std::pair< int, int > pair( 10, 100 );
		SERIALIZER_PUT_VARIABLE( writer, pair );
	}

	{
		Reader reader( filename );

		std::pair< int, int > pair( 10, 100 );
		SERIALIZER_GET_VARIABLE( reader, pair );

		EXPECT_EQ( 10, pair.first );
		EXPECT_EQ( 100, pair.second );
	}
}

TextBinaryTest( StdPair );

template< typename Reader, typename Writer >
void StdMap( const char *filename  ) {
	{
		Writer writer( filename );

		std::map< int, int > map;
		for( int i = 0 ; i < 100 ; ++i ) {
			map[i] = 2*i;
		}

		SERIALIZER_PUT_VARIABLE( writer, map );
	}

	{
		Reader reader( filename );

		std::map< int, int > map;

		SERIALIZER_GET_VARIABLE( reader, map );

		for( int i = 0 ; i < 100 ; ++i ) {
			EXPECT_EQ( 2*i, map[i] );
		}
	}
}

TextBinaryTest( StdMap );

namespace StdMapTests {
struct NonFundamental {
	int x;
	NonFundamental( int x = 0 ) : x( x ) {}

	SERIALIZER_DEFAULT_IMPL( (x) );

	friend bool operator <( const NonFundamental &a, const NonFundamental &b ) {
		return a.x < b.x;
	}
};

template< typename Reader, typename Writer >
void StdMap_ComplexKey( const char *filename  ) {
	{
		Writer writer( filename );

		std::map< NonFundamental, int > map;
		for( int i = 0 ; i < 2 ; ++i ) {
			map[ NonFundamental( i ) ] = 2*i;
		}

		SERIALIZER_PUT_VARIABLE( writer, map );
	}

	{
		Reader reader( filename );

		std::map< NonFundamental, int > map;

		SERIALIZER_GET_VARIABLE( reader, map );

		for( int i = 0 ; i < 2 ; ++i ) {
			EXPECT_EQ( 2*i, map[ i ] );
		}
	}
}

TextBinaryTest( StdMap_ComplexKey );

template< typename Reader, typename Writer >
void StdMap_ComplexValue( const char *filename  ) {
	{
		Writer writer( filename );

		std::map< int, NonFundamental > map;
		for( int i = 0 ; i < 2 ; ++i ) {
			map[ i ] = 2*i;
		}

		SERIALIZER_PUT_VARIABLE( writer, map );
	}

	{
		Reader reader( filename );

		std::map< int, NonFundamental > map;

		SERIALIZER_GET_VARIABLE( reader, map );

		for( int i = 0 ; i < 2 ; ++i ) {
			EXPECT_EQ( 2*i, map[ i ].x );
		}
	}
}

TextBinaryTest( StdMap_ComplexValue );
}

// test raw mode

struct RawStruct {
	char c;
	int x;

	SERIALIZER_ENABLE_RAW_MODE();
};

template< typename Reader, typename Writer >
void RawMode( const char *filename  ) {
	{
		Writer writer( filename );

		RawStruct s = { 'a', 10 };
		RawStruct zero = { 0, 0 };
		SERIALIZER_PUT_VARIABLE( writer, s );
		SERIALIZER_PUT_VARIABLE( writer, zero );
	}

	{
		Reader reader( filename );

		RawStruct s, zero;
		SERIALIZER_GET_VARIABLE( reader, s );
		SERIALIZER_GET_VARIABLE( reader, zero );

		EXPECT_EQ( 'a', s.c );
		EXPECT_EQ( 10, s.x );
		EXPECT_EQ( 0, zero.c );
		EXPECT_EQ( 0, zero.x );
	}
}

TextBinaryTest( RawMode );

struct RawStructExtern {
	char c;
	int x;
};

SERIALIZER_ENABLE_RAW_MODE_EXTERN( RawStructExtern );

template< typename Reader, typename Writer >
void RawModeExtern( const char *filename  ) {
	{
		Writer writer( filename );

		RawStructExtern s = { 'a', 10 };
		SERIALIZER_PUT_VARIABLE( writer, s );
	}

	{
		Reader reader( filename );

		RawStructExtern s;
		SERIALIZER_GET_VARIABLE( reader, s );

		EXPECT_EQ( 'a', s.c );
		EXPECT_EQ( 10, s.x );
	}
}

TextBinaryTest( RawModeExtern );

struct RawNonFundamental {
	int x;

	RawNonFundamental() {}
	RawNonFundamental( int x ) : x( x ) {}
};

void read( Serializer::BinaryReader &reader, RawNonFundamental &rnf ) {
	FAIL();
}

void write( Serializer::BinaryWriter &writer, const RawNonFundamental &rnf ) {
	FAIL();
}

SERIALIZER_ENABLE_RAW_MODE_EXTERN( RawNonFundamental );

template< typename Reader, typename Writer >
void StdVector_RawNonFundamental( const char *filename ) {
	{
		Writer writer( filename );

		std::vector<RawNonFundamental> seq;

		for( int i = 0 ; i < 100 ; i++ )
			seq.push_back( i );

		EXPECT_NO_FATAL_FAILURE( SERIALIZER_PUT_VARIABLE( writer, seq ) );
	}

	{
		Reader reader( filename );

		std::vector<RawNonFundamental> seq;

		EXPECT_NO_FATAL_FAILURE( SERIALIZER_GET_VARIABLE( reader, seq ) );

		for( int i = 0 ; i < 100 ; i++ )
			ASSERT_EQ( i, seq[i].x );
	}
}

TextBinaryTest( StdVector_RawNonFundamental );

// std::list
template< typename Reader, typename Writer >
void StdList( const char *filename ) {
	{
		Writer writer( filename );

		std::list<int> seq;

		for( int i = 0 ; i < 100 ; i++ )
			seq.push_back( i );

		SERIALIZER_PUT_VARIABLE( writer, seq );
	}

	{
		Reader reader( filename );

		std::list<int> seq;

		SERIALIZER_GET_VARIABLE( reader, seq );

		for( int i = 0 ; i < 100 ; i++ ) {
			ASSERT_EQ( i, seq.front() );
			seq.pop_front();
		}
	}
}

TextBinaryTest( StdList );

//////////////////////////////////////////////////////////////////////////
// eigen library support
#include "serializer_eigen.h"

template< typename EigenType >
struct GenericEigenMatrixTest {
	template< typename Reader, typename Writer >
	static void Test( const char *filename  ) {
		EigenType w;
		w.setRandom();

		{
			Writer writer( filename );

			EigenType v = w;

			SERIALIZER_PUT_VARIABLE( writer, v );
		}

		{
			Reader reader( filename );

			EigenType v;
			SERIALIZER_GET_VARIABLE( reader, v );

			EXPECT_TRUE( v.isApprox( w ) );
		}
	}
};

#define TextBinaryTest_EigenMatrix( type ) TextBinaryTestEx( GenericEigenMatrixTest<Eigen:: type >::Test, type )

TextBinaryTest_EigenMatrix( Matrix3f );
TextBinaryTest_EigenMatrix( Matrix4f );
TextBinaryTest_EigenMatrix( Matrix2f );

TextBinaryTest_EigenMatrix( Vector2f );
TextBinaryTest_EigenMatrix( Vector3f );
TextBinaryTest_EigenMatrix( Vector4f );

TextBinaryTest_EigenMatrix( RowVector2f );
TextBinaryTest_EigenMatrix( RowVector3f );
TextBinaryTest_EigenMatrix( RowVector4f );

TextBinaryTest_EigenMatrix( Matrix3d );
TextBinaryTest_EigenMatrix( Matrix4d );
TextBinaryTest_EigenMatrix( Matrix2d );

TextBinaryTest_EigenMatrix( Vector2d );
TextBinaryTest_EigenMatrix( Vector3d );
TextBinaryTest_EigenMatrix( Vector4d );

TextBinaryTest_EigenMatrix( RowVector2d );
TextBinaryTest_EigenMatrix( RowVector3d );
TextBinaryTest_EigenMatrix( RowVector4d );

template< typename EigenType >
struct GenericEigenAlignedBoxTest {
	template< typename Reader, typename Writer >
	static void Test( const char *filename  ) {
		EigenType ref;

		ref.min().setRandom();
		ref.max().setRandom();

		{
			Writer writer( filename );

			EigenType box = ref;

			SERIALIZER_PUT_VARIABLE( writer, box );
		}

		{
			Reader reader( filename );

			EigenType box;
			SERIALIZER_GET_VARIABLE( reader, box );

			EXPECT_TRUE( box.isApprox( ref ) );
		}
	}
};

#define TextBinaryTest_EigenAlignedBox( type ) TextBinaryTestEx( GenericEigenAlignedBoxTest<Eigen:: type >::Test, type )

TextBinaryTest_EigenAlignedBox( AlignedBox2f );
TextBinaryTest_EigenAlignedBox( AlignedBox3f );
TextBinaryTest_EigenAlignedBox( AlignedBox4f );

TextBinaryTest_EigenAlignedBox( AlignedBox2d );
TextBinaryTest_EigenAlignedBox( AlignedBox3d );
TextBinaryTest_EigenAlignedBox( AlignedBox4d );

template< typename EigenType >
struct GenericEigenTransformTest {
	template< typename Reader, typename Writer >
	static void Test( const char *filename  ) {
		EigenType ref;

		ref.matrix().setRandom();

		{
			Writer writer( filename );

			EigenType transform = ref;

			SERIALIZER_PUT_VARIABLE( writer, transform );
		}

		{
			Reader reader( filename );

			EigenType transform;
			SERIALIZER_GET_VARIABLE( reader, transform );

			EXPECT_TRUE( transform.isApprox( ref ) );
		}
	}
};

#define TextBinaryTest_EigenTransform( type ) TextBinaryTestEx( GenericEigenTransformTest<Eigen:: type >::Test, type )

TextBinaryTest_EigenTransform( Isometry2f );
TextBinaryTest_EigenTransform( Isometry3f );
TextBinaryTest_EigenTransform( Isometry2d );
TextBinaryTest_EigenTransform( Isometry3d );

TextBinaryTest_EigenTransform( Affine2f );
TextBinaryTest_EigenTransform( Affine3f );
TextBinaryTest_EigenTransform( Affine2d );
TextBinaryTest_EigenTransform( Affine3d );

TextBinaryTest_EigenTransform( AffineCompact2f );
TextBinaryTest_EigenTransform( AffineCompact3f );
TextBinaryTest_EigenTransform( AffineCompact2d );
TextBinaryTest_EigenTransform( AffineCompact3d );

TextBinaryTest_EigenTransform( Projective2f );
TextBinaryTest_EigenTransform( Projective3f );
TextBinaryTest_EigenTransform( Projective2d );
TextBinaryTest_EigenTransform( Projective3d );

//////////////////////////////////////////////////////////////////////////
// detail tests

BOOST_STATIC_ASSERT( Serializer::detail::is_default_constructible<int>::value );
BOOST_STATIC_ASSERT( Serializer::detail::is_default_constructible<bool>::value );
BOOST_STATIC_ASSERT( Serializer::detail::is_default_constructible<std::string>::value );
BOOST_STATIC_ASSERT( !Serializer::detail::is_default_constructible<int[100]>::value );

BOOST_STATIC_ASSERT( Serializer::detail::is_default_constructible<const std::string>::value );

struct NotDefaultConstructible {
	const int x;
	NotDefaultConstructible( int a ) : x(a) {}
};

BOOST_STATIC_ASSERT( !Serializer::detail::is_default_constructible<NotDefaultConstructible>::value );

struct DefaultConstructible {
	const int x;

	DefaultConstructible() : x(0) {}
};

BOOST_STATIC_ASSERT( Serializer::detail::is_default_constructible<DefaultConstructible>::value );

//////////////////////////////////////////////////////////////////////////
// move-only object

#include <boost/noncopyable.hpp>

namespace MoveOnly {
	struct MO : boost::noncopyable {
		int x;
		int y;

		MO() {}
		MO( int x ) : x( x ), y( y ) {}
		MO( MO &&other ) : x( other.x ), y( other.y ) {}

		MO & operator = ( MO &&other ) {
			x = other.x;
			y = other.y;

			return *this;
		}

		SERIALIZER_DEFAULT_IMPL( (x)(y) )
	};

	template< typename Reader, typename Writer >
	void MoveOnly( const char *filename ) {
		std::vector< MO > ref;

		for( int i = 0 ; i < 100 ; i++ ) {
			ref.emplace_back( MO(i) );
		}

		{
			Writer writer( filename );

			Serializer::put( writer, ref );
		}

		{
			Reader reader( filename );

			decltype( ref ) test;
			Serializer::get( reader, test );

			for( int i = 0 ; i < 100 ; i++ ) {
				ASSERT_EQ( test[i].x, ref[i].x );
				ASSERT_EQ( test[i].y, ref[i].y );
			}
		}
	}

	TextBinaryTest( MoveOnly );
}
