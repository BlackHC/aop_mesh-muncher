#include <Eigen/Eigen>
//#include <gtest/gtest.h>

void pseudoinverse (const double (*in)[3], double (*out)[3], int size)
{
	double work[3][6], num;
	int i, j, k;

	for (i=0; i < 3; i++) {
		for (j=0; j < 6; j++)
			work[i][j] = j == i+3;
		for (j=0; j < 3; j++)
			for (k=0; k < size; k++)
				work[i][j] += in[k][i] * in[k][j];
	}
	for (i=0; i < 3; i++) {
		num = work[i][i];
		for (j=0; j < 6; j++)
			work[i][j] /= num;
		for (k=0; k < 3; k++) {
			if (k==i) continue;
			num = work[k][i];
			for (j=0; j < 6; j++)
				work[k][j] -= work[i][j] * num;
		}
	}
	for (i=0; i < size; i++)
		for (j=0; j < 3; j++)
			for (out[i][j]=k=0; k < 3; k++)
				out[i][j] += work[j][k+3] * in[i][k];
}


// To the extent possible under law, Manuel Llorens <manuelllorens@gmail.com>
// has waived all copyright and related or neighboring rights to this work.
// This code is licensed under CC0 v1.0, see license information at
// http://creativecommons.org/publicdomain/zero/1.0/

double ep = 216.0/24389.0,ka = 24389.0/27.0;
double xyz_rgb_2[3][3];
double d50_white[3]={0.964220,1,0.825211};
// row major
static double rgb_xyz[3][3] =
{ { 0.7976748465, 0.1351917082, 0.0313534088 },
{ 0.2880402025, 0.7118741325, 0.0000856651 },
{ 0.0000000000, 0.0000000000, 0.8252114389 } }; // From Jacques Desmis

double f_cbrt(double r){
	return r>ep?pow(r,1/3.0):(ka*r+16)/116.0;
}

void CIELab_Init(){
	// TODO: compute with mathematica
	pseudoinverse(rgb_xyz,xyz_rgb_2,3);
}

Eigen::Vector3f RGB_to_CIELab( const Eigen::Vector3f &rgb ) {
	Eigen::Vector3d lab, xyz;

	// Convert RGB to XYZ
	xyz = Eigen::Map< Eigen::Matrix<double, 3,3, Eigen::RowMajor | Eigen::DontAlign > >( &rgb_xyz[0][0] ) * rgb.cast<double>();

	xyz.cwiseQuotient( Eigen::Map<Eigen::Vector3d >( d50_white ) );
	
	// Convert XYZ to L*a*b*
	for (int c=0; c < 3; c++)
		xyz[c]=f_cbrt(xyz[c]/d50_white[c]);

	lab[0]=116.0*xyz[1]-16.0;
	lab[1]=500.0*(xyz[0]-xyz[1]);
	lab[2]=200.0*(xyz[1]-xyz[2]);

	return lab.cast<float>();
}

Eigen::Vector3f CIELab_to_RGB( const Eigen::Vector3f &lab ){
	Eigen::Vector3d xyz, rgb, f;

	// Convert L*a*b* to XYZ
	double L=(double)lab[0];
	f[1]=(L+16.0)/116.0;	// fy
	f[0]=f[1]+lab[1]/500.0;	// fx
	f[2]=f[1]-lab[2]/200.0;	// fz

	xyz[0]=d50_white[0]*(f[0]*f[0]*f[0]>ep?f[0]*f[0]*f[0]:(116.0*f[0]-16.0)/ka);
	xyz[1]=d50_white[1]*(L>ka*ep?pow(f[1],3.0):L/ka);
	xyz[2]=d50_white[2]*(f[2]*f[2]*f[2]>ep?f[2]*f[2]*f[2]:(116.0*f[2]-16.0)/ka);

	// Convert XYZ to RGB
	rgb = Eigen::Map< Eigen::Matrix<double, 3,3,Eigen::ColMajor | Eigen::DontAlign> >( &xyz_rgb_2[0][0] ) * xyz;

	return rgb.cast<float>();
}

/*TEST(CIELab, conversion) {
	CIELab_Init();

#define INV_TEST( v, err ) EXPECT_NEAR( (v), RGB_to_CIELab( CIELab_to_RGB( v ) ), (err) )
	INV_TEST( Eigen::Vector3f::Constant( 1.0 ) );
	INV_TEST( Eigen::Vector3f::Constant( 0.5 ) );
	INV_TEST( Eigen::Vector3f::Unit(0) );
	INV_TEST( Eigen::Vector3f::Unit(1) );
	INV_TEST( Eigen::Vector3f::Unit(2) );
}*/