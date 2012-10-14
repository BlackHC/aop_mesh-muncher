#include <Eigen/Eigen>

// To the extent possible under law, Manuel Llorens <manuelllorens@gmail.com>
// has waived all copyright and related or neighboring rights to this work.
// This code is licensed under CC0 v1.0, see license information at
// http://creativecommons.org/publicdomain/zero/1.0/

static float ep = 6.0f*6*6/(29*29*29),ka = 29.0f*29*29/(3*3*3);
//static float d50_white[3]={0.964220,1,0.825211};
static float d65_white[3]={0.95047f,1.0f,1.08883f};
/*// row major
static double xyz_rgb_2[3][3] = {
	{   1.34594347, -0.255607541,-0.0511117722},
	{ -0.544598842,   1.50816724, 0.0205351405},
	{            0,            0,   1.21181064}
};
// row major
static double rgb_xyz[3][3] = {
	{ 0.7976748465, 0.1351917082, 0.0313534088 },
	{ 0.2880402025, 0.7118741325, 0.0000856651 },
	{ 0.0000000000, 0.0000000000, 0.8252114389 }
}; // From Jacques Desmis*/
// according to wikipedia: The numerical values below match those in the official sRGB specification (IEC 61966-2-1:1999) and differ slightly from those in a publication by sRGB's creators.
// {http://en.wikipedia.org/wiki/SRGB}
// xyz -> linear RGB
static float xyz_rgb[3][3] = /*{
	{ 3.2410, -1.5374, -0.4986 },
	{ -0.9692, 1.8760, 0.0416 },
	{ 0.0556, -0.2040, 1.0570 }
};*/
// row major
{{  3.24062514f, -1.53720784f,-0.498628557f},
{-0.968930602f,  1.87575603f,0.0415175036f},
{0.0557101034f,-0.204021037f,  1.05699599f}};
// row major
static float rgb_xyz[3][3] = {
	{ 0.4124f, 0.3576f, 0.1805f },
	{ 0.2126f, 0.7152f, 0.0722f },
	{ 0.0193f, 0.1192f, 0.9505f }
};

inline static float f_cbrt(const float r){
	return r > ep ? pow(r, 1/3.0f) : (ka*r+16)/116.0f;
}

inline static float inv_f(const float w) {
	return w > 6.0f/29 ? w*w*w : (w*116.0f-16.0f)/ka;
}

inline static 
	Eigen::Map< Eigen::Matrix<float, 3,3, Eigen::RowMajor | Eigen::DontAlign > > 
eigenRGBtoXYZ() {
	return Eigen::Map< Eigen::Matrix<float, 3,3, Eigen::RowMajor | Eigen::DontAlign > >( &rgb_xyz[0][0] );
}

inline static
	Eigen::Map< Eigen::Matrix<float, 3,3,Eigen::RowMajor | Eigen::DontAlign> >
eigenXYZtoRGB() {
	return Eigen::Map< Eigen::Matrix<float, 3,3,Eigen::RowMajor | Eigen::DontAlign> >( &xyz_rgb[0][0] );
}

inline static 
	Eigen::Map< Eigen::Vector3f >
eigenD65White() {
	return Eigen::Vector3f::Map( d65_white );
}

namespace ColorConversion {
	Eigen::Vector3f RGB_to_CIELab( const Eigen::Vector3f &rgb ) {
		Eigen::Vector3f lab, xyz;

		// Convert RGB to XYZ
		xyz = eigenRGBtoXYZ() * rgb;

		// Convert XYZ to L*a*b*
		for (int c=0; c < 3; c++)
			xyz[c]=f_cbrt(xyz[c]/d65_white[c]);

		lab[0]=116.0f*xyz[1]-16.0f;
		lab[1]=500.0f*(xyz[0]-xyz[1]);
		lab[2]=200.0f*(xyz[1]-xyz[2]);

		return lab;
	}

	Eigen::Vector3f CIELab_to_RGB( const Eigen::Vector3f &lab ){
		Eigen::Vector3f xyz, rgb, f;

		// Convert L*a*b* to XYZ
		f[1]=(lab[0]+16.0f)/116.0f;
		f[0]=f[1]+lab[1]/500.0f;
		f[2]=f[1]-lab[2]/200.0f;

		xyz[0]=d65_white[0]*inv_f(f[0]);
		xyz[1]=d65_white[1]*inv_f(f[1]);
		xyz[2]=d65_white[2]*inv_f(f[2]);

		// Convert XYZ to RGB
		rgb = eigenXYZtoRGB() * xyz;

		return rgb;
	}

#if 0
	// Ive forgotten about the t^(1/3)

	namespace Approximation {
		// row major
		float rgb2xyz[3][3] = {
			{ 24.6615982, 82.9632034, 8.37520027},
			{ 89.6869125,-187.655975, 49.6799164},
			{ 38.317112, 117.082306, -192.54657}
		};

		// row major
		float xyz2rgb[3][3] = {
			{ 0.0121925958,  0.00681899581,  0.00228974503},
			{ 0.00771089504, -0.00203884533,-0.000190651961},
			{ 0.00711513218, 0.000117226366, -0.00485381531}
		};

		inline static 
			Eigen::Map< Eigen::Matrix<float, 3,3, Eigen::RowMajor | Eigen::DontAlign > > 
		eigenRGBtoXYZ() {
			return Eigen::Map< Eigen::Matrix<float, 3,3, Eigen::RowMajor | Eigen::DontAlign > >( &rgb2xyz[0][0] );
		}

		inline static
			Eigen::Map< Eigen::Matrix<float, 3,3,Eigen::RowMajor | Eigen::DontAlign> >
		eigenXYZtoRGB() {
			return Eigen::Map< Eigen::Matrix<float, 3,3,Eigen::RowMajor | Eigen::DontAlign> >( &xyz2rgb[0][0] );
		}

		Eigen::Vector3f RGB_to_CIELab( const Eigen::Vector3f &rgb ) {
			return eigenRGBtoXYZ() * rgb + Eigen::Vector3f( -16.0, 0.0, 0.0 );
		}

		Eigen::Vector3f CIELab_to_RGB( const Eigen::Vector3f &lab ) {
			return eigenXYZtoRGB() * (lab + Eigen::Vector3f( 16.0, 0.0, 0.0 ));
		}
	}
#endif
}

#ifdef CIELAB_GTEST_UNIT_TESTS
#include <gtest.h>

using namespace Eigen;
using namespace ColorConversion;

static std::ostream & operator << (std::ostream & s, const Matrix3f & m) {
	return Eigen::operator<<( s, m );
}

static std::ostream & operator << (std::ostream & s, const Vector3f & m) {
	return Eigen::operator<<( s, m );
}

TEST(CIELab, inversionTest) {
#define INV_TEST( v ) /*std::cout << v << "\n vs \n" << RGB_to_CIELab( CIELab_to_RGB( v ) ) << "\n\n";*/ EXPECT_TRUE( v.isApprox( RGB_to_CIELab( CIELab_to_RGB( v ) ), 0.001 ) )
	INV_TEST( Eigen::Vector3f::Constant( 1.0 ) );
	INV_TEST( Eigen::Vector3f::Constant( 0.5 ) );
	INV_TEST( Eigen::Vector3f::Unit(0) );
	INV_TEST( Eigen::Vector3f::Unit(1) );
	INV_TEST( Eigen::Vector3f::Unit(2) );
}

TEST(CIELab, rangeConstants ) {
	EXPECT_TRUE( RGB_to_CIELab( Eigen::Vector3f( 1.0, 1.0, 1.0 ) ).isApprox( Eigen::Vector3f( 100, 0.0, 0.0 ), 0.1 ) );
	//std::cout << RGB_to_CIELab( Eigen::Vector3f( 1.0, 1.0, 1.0 ) );
	EXPECT_TRUE( RGB_to_CIELab( Eigen::Vector3f( 0.0, 0.0, 0.0 ) ).isZero( 0.1 ) );
	//std::cout << RGB_to_CIELab( Eigen::Vector3f( 1.0, 0.0, 0.0 ) );
	EXPECT_TRUE( RGB_to_CIELab( Eigen::Vector3f( 1.0, 0.0, 0.0 ) ).isApprox( Eigen::Vector3f( 53.0, 80.0, 67.0 ), 0.1 ) );
	EXPECT_TRUE( RGB_to_CIELab( Eigen::Vector3f( 0.0, 1.0, 0.0 ) ).isApprox( Eigen::Vector3f( 88.0, -86.0, 83.0 ), 0.1 ) );
	EXPECT_TRUE( RGB_to_CIELab( Eigen::Vector3f( 0.0, 0.0, 1.0 ) ).isApprox( Eigen::Vector3f( 32.0, 79.0, -108.0 ), 0.1 ) );
}

TEST( CIELAB, testMatrices ) {
	auto rgb2xyz = Eigen::Map< Eigen::Matrix<float, 3,3,Eigen::RowMajor | Eigen::DontAlign> >( &rgb_xyz[0][0] );

#if 0
	std::cout << rgb2xyz.inverse().format( Eigen::IOFormat( 9, 0,",", ",\n", "{", "}", "{", "}" ) );
#endif 
	auto xyz2rgb = Eigen::Map< Eigen::Matrix<float, 3,3,Eigen::RowMajor | Eigen::DontAlign> >( &xyz_rgb[0][0] );

	//std::cout << rgb2xyz * xyz2rgb;
	EXPECT_TRUE( (rgb2xyz * xyz2rgb).isIdentity( 0.0001 ) );
}

#if 0
TEST( CIELAB_Approx, calcApproximation ) {
	Eigen::Matrix3f xyzToLab;
	
	// THIS IS MISSING THE -16.0f SHIFT IN lab[0]!!
	xyzToLab << 
		0.0, 116.0, 0,
		500.0, -500.0, 0.0,
		0.0, 200.0, -200.0;
	

	auto appoxRGBtoLAB = xyzToLab * Eigen::Scaling( eigenD65White() ) * eigenRGBtoXYZ();
	std::cout 
		<< "appoxRGBtoLAB (wo shift) = \n" 
		<< appoxRGBtoLAB.format( Eigen::IOFormat( 9, 0,",", ",\n", "{", "}", "{", "}" ) )
		<< "\n"
		<< "appoxLABtoRGB (wo shift) = \n" 
		<< appoxRGBtoLAB.inverse().format( Eigen::IOFormat( 9, 0,",", ",\n", "{", "}", "{", "}" ) )
		<< "\n";

	EXPECT_TRUE( Approximation::eigenRGBtoXYZ().isApprox( appoxRGBtoLAB ) );
	EXPECT_TRUE( Approximation::eigenXYZtoRGB().isApprox( appoxRGBtoLAB.inverse() ) );
	
	EXPECT_TRUE( (Approximation::eigenRGBtoXYZ() * Approximation::eigenXYZtoRGB()).isIdentity() );
	EXPECT_TRUE( (Approximation::eigenXYZtoRGB() * Approximation::eigenRGBtoXYZ()).isIdentity() );
}

TEST( CIELAB_Approx, compare ) {
#define PRINT_DIFF(a,b) std::cout << "d( " << a.transpose() << ", approxLabToRGB^-1( RGBToLab( . ) ) ) = " << ( a - Approximation::CIELab_to_RGB( RGB_to_CIELab( a ) ) ).norm() << "\n";
	PRINT_DIFF( Eigen::Vector3f::Unit(0) );
	PRINT_DIFF( Eigen::Vector3f::Unit(1) );
	PRINT_DIFF( Eigen::Vector3f::Unit(2) );
}
#endif 
TEST(CIELab, printColorCalibration) {
#define PRINT_DIFF(a,b) std::cout << "d( " << a.transpose() << ", " << b.transpose() << " ) = " << ( RGB_to_CIELab( a ) - RGB_to_CIELab( b ) ).norm() << "\n";
	PRINT_DIFF( Eigen::Vector3f::Unit(0), 0.75 * Eigen::Vector3f::Unit(0) );
	PRINT_DIFF( Eigen::Vector3f::Unit(1), 0.75 * Eigen::Vector3f::Unit(1) );
	PRINT_DIFF( Eigen::Vector3f::Unit(2), 0.75 * Eigen::Vector3f::Unit(2) );

	PRINT_DIFF( 0.5 * Eigen::Vector3f::Unit(0), 0.75 * Eigen::Vector3f::Unit(0) );
	PRINT_DIFF( 0.5 * Eigen::Vector3f::Unit(1), 0.75 * Eigen::Vector3f::Unit(1) );
	PRINT_DIFF( 0.5 * Eigen::Vector3f::Unit(2), 0.75 * Eigen::Vector3f::Unit(2) );
	
	PRINT_DIFF( 0.25 * Eigen::Vector3f::Unit(0), 0.5 * Eigen::Vector3f::Unit(0) );
	PRINT_DIFF( 0.25 * Eigen::Vector3f::Unit(1), 0.5 * Eigen::Vector3f::Unit(1) );
	PRINT_DIFF( 0.25 * Eigen::Vector3f::Unit(2), 0.5 * Eigen::Vector3f::Unit(2) );

	PRINT_DIFF( Eigen::Vector3f::Unit(0), 0.5 * Eigen::Vector3f::Unit(0) );
	PRINT_DIFF( Eigen::Vector3f::Unit(1), 0.5 * Eigen::Vector3f::Unit(1) );
	PRINT_DIFF( Eigen::Vector3f::Unit(2), 0.5 * Eigen::Vector3f::Unit(2) );

	PRINT_DIFF( Eigen::Vector3f::Zero(), 0.25 * Eigen::Vector3f::Unit(0) );
	PRINT_DIFF( Eigen::Vector3f::Zero(), 0.25 * Eigen::Vector3f::Unit(1) );
	PRINT_DIFF( Eigen::Vector3f::Zero(), 0.25 * Eigen::Vector3f::Unit(2) );

	PRINT_DIFF( Eigen::Vector3f::Unit(0), Eigen::Vector3f::Unit(1) );
	PRINT_DIFF( Eigen::Vector3f::Unit(0), Eigen::Vector3f::Unit(2) );
	PRINT_DIFF( Eigen::Vector3f::Unit(1), Eigen::Vector3f::Unit(2) );
}
#endif