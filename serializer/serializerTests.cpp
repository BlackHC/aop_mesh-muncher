#include "gtest.h"

#include "serializer.h"

// pure tests

const char *textFilename = "textTestExchange.txt";
const char *binaryFilename = "binaryTestExchange.bin";
/*
template:

TEST( TextSerialization,  ) {
	{
		Serializer::TextWriter writer( textFilename );
	}

	{
		Serializer::TextReader reader( textFilename );
	}
}

TEST( BinarySerialization,  ) {
	{
		Serializer::BinaryWriter writer( binaryFilename );
	}

	{
		Serializer::BinaryReader reader( binaryFilename );
	}
}

*/

TEST( TextSerialization, arithmeticSerializations ) {
	{
		int i = 10;
		float f = 3.1415;
		char c = 'a';

		Serializer::TextWriter writer( textFilename );
		SERIALIZER_PUT_VARIABLE( writer, i );
		SERIALIZER_PUT_VARIABLE( writer, f );
		SERIALIZER_PUT_VARIABLE( writer, c );
	}

	{
		int i;
		float f;
		char c;

		Serializer::TextReader reader( textFilename );
		SERIALIZER_GET_VARIABLE( reader, i );
		SERIALIZER_GET_VARIABLE( reader, f );
		SERIALIZER_GET_VARIABLE( reader, c );

		EXPECT_EQ( 10, i );
		EXPECT_FLOAT_EQ( 3.1415, f );
		EXPECT_EQ( 'a', c );
	}
}

TEST( BinarySerialization, arithmeticSerializations ) {
	{
		int i = 10;
		float f = 3.1415;
		char c = 'a';

		Serializer::BinaryWriter writer( binaryFilename );
		SERIALIZER_PUT_VARIABLE( writer, i );
		SERIALIZER_PUT_VARIABLE( writer, f );
		SERIALIZER_PUT_VARIABLE( writer, c );
	}

	{
		int i;
		float f;
		char c;

		Serializer::BinaryReader reader( binaryFilename );
		SERIALIZER_GET_VARIABLE( reader, i );
		SERIALIZER_GET_VARIABLE( reader, f );
		SERIALIZER_GET_VARIABLE( reader, c );

		EXPECT_EQ( 10, i );
		EXPECT_FLOAT_EQ( 3.1415, f );
		EXPECT_EQ( 'a', c );
	}
}

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

TEST( TextSerialization, GlobalStruct ) {
	{
		Serializer::TextWriter writer( textFilename );

		GlobalStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Serializer::TextReader reader( textFilename );

		GlobalStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TEST( BinarySerialization, GlobalStruct ) {
	{
		Serializer::BinaryWriter writer( binaryFilename );

		GlobalStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Serializer::BinaryReader reader( binaryFilename );

		GlobalStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

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

TEST( TextSerialization, MemberStruct ) {
	{
		Serializer::TextWriter writer( textFilename );

		MemberStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Serializer::TextReader reader( textFilename );

		MemberStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TEST( BinarySerialization, MemberStruct ) {
	{
		Serializer::BinaryWriter writer( binaryFilename );

		MemberStruct x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Serializer::BinaryReader reader( binaryFilename );

		MemberStruct x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

// macro test
struct MacroTest {
	int x, y;

	SERIALIZER_DEFAULT_IMPL( (x)(y) )
};

TEST( TextSerialization, MacroTest ) {
	{
		Serializer::TextWriter writer( textFilename );

		MacroTest x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Serializer::TextReader reader( textFilename );

		MacroTest x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TEST( BinarySerialization, MacroTest ) {
	{
		Serializer::BinaryWriter writer( binaryFilename );

		MacroTest x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Serializer::BinaryReader reader( binaryFilename );

		MacroTest x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

struct ExternMacroTest {
	int x, y;
};

SERIALIZER_DEFAULT_EXTERN_IMPL( ExternMacroTest, (x)(y) )

TEST( TextSerialization, ExternMacroTest ) {
	{
		Serializer::TextWriter writer( textFilename );

		ExternMacroTest x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Serializer::TextReader reader( textFilename );

		ExternMacroTest x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

TEST( BinarySerialization, ExternMacroTest ) {
	{
		Serializer::BinaryWriter writer( binaryFilename );

		ExternMacroTest x = { 10, 100 };
		Serializer::put( writer, "x", x );
	}

	{
		Serializer::BinaryReader reader( binaryFilename );

		ExternMacroTest x;
		Serializer::get( reader, "x", x );

		EXPECT_EQ( 10, x.x );
		EXPECT_EQ( 100, x.y );
	}
}

// std tests

#include "serializer_std.h"

const char *scratchFilename = "testScratch";

/*
template< typename Reader, typename Writer >
void genericTest() {
	{
		Writer writer( scratchFilename );
	}

	{
		Reader reader( scratchFilename );
	}
}*/

#define TextBinaryTestEx( test, name ) \
	TEST( name, TextSerialization ) { \
		test < Serializer::TextReader, Serializer::TextWriter >(); \
	} \
	 \
	TEST( name, BinarySerialization ) { \
		test < Serializer::BinaryReader, Serializer::BinaryWriter >(); \
	}

#define TextBinaryTest( test ) TextBinaryTestEx( test, test )

template< typename Reader, typename Writer >
void StdString() {
	{		
		Writer writer( scratchFilename );

		std::string text = "hello world\nhallo welt";
		SERIALIZER_PUT_VARIABLE( writer, text );
	}

	{	
		Reader reader( scratchFilename );

		std::string text;
		SERIALIZER_GET_VARIABLE( reader, text );

		EXPECT_EQ( "hello world\nhallo welt", text );
	}
}

TextBinaryTest( StdString );

template< typename Reader, typename Writer >
void StdVector() {
	{
		Writer writer( scratchFilename );

		std::vector<int> seq;

		for( int i = 0 ; i < 100 ; i++ )
			seq.push_back( i );

		SERIALIZER_PUT_VARIABLE( writer, seq );			
	}

	{
		Reader reader( scratchFilename );

		std::vector<int> seq;

		SERIALIZER_GET_VARIABLE( reader, seq );	

		for( int i = 0 ; i < 100 ; i++ )
			ASSERT_EQ( i, seq[i] );		
	}
}

TextBinaryTest( StdVector );

template< typename Reader, typename Writer >
void StdPair() {
	{
		Writer writer( scratchFilename );

		std::pair< int, int > pair( 10, 100 );
		SERIALIZER_PUT_VARIABLE( writer, pair );
	}

	{
		Reader reader( scratchFilename );
		
		std::pair< int, int > pair( 10, 100 );
		SERIALIZER_GET_VARIABLE( reader, pair );

		EXPECT_EQ( 10, pair.first );
		EXPECT_EQ( 100, pair.second );
	}
}

TextBinaryTest( StdPair );

template< typename Reader, typename Writer >
void StdMap() {
	{
		Writer writer( scratchFilename );

		std::map< int, int > map;
		for( int i = 0 ; i < 100 ; ++i ) {
			map[i] = 2*i;
		}

		SERIALIZER_PUT_VARIABLE( writer, map );
	}

	{
		Reader reader( scratchFilename );

		std::map< int, int > map;

		SERIALIZER_GET_VARIABLE( reader, map );

		for( int i = 0 ; i < 100 ; ++i ) {
			EXPECT_EQ( 2*i, map[i] );
		}
	}
}

TextBinaryTest( StdMap );

// test raw mode

struct RawStruct {
	char c;
	int x;
};

SERIALIZER_ENABLE_RAWMODE( RawStruct );

template< typename Reader, typename Writer >
void RawMode() {
	{
		Writer writer( scratchFilename );

		RawStruct s = { 'a', 10 };
		SERIALIZER_PUT_VARIABLE( writer, s );
	}

	{
		Reader reader( scratchFilename );

		RawStruct s;
		SERIALIZER_GET_VARIABLE( reader, s );

		EXPECT_EQ( 'a', s.c );
		EXPECT_EQ( 10, s.x );
	}
}

TextBinaryTest( RawMode );

// eigen library support
#include "serializer_eigen.h"

template< typename EigenType >
struct GenericEigenMatrixTest {
	template< typename Reader, typename Writer >
	static void Test() {
		EigenType w;
		w.setRandom();

		{
			Writer writer( scratchFilename );

			EigenType v = w;
		
			SERIALIZER_PUT_VARIABLE( writer, v );
		}

		{
			Reader reader( scratchFilename );

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
	static void Test() {
		EigenType ref;

		ref.min().setRandom();
		ref.max().setRandom();

		{
			Writer writer( scratchFilename );

			EigenType box = ref;

			SERIALIZER_PUT_VARIABLE( writer, box );
		}

		{
			Reader reader( scratchFilename );

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
