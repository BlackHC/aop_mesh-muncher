#pragma once

#include <Eigen/Eigen>

struct OBB {
	typedef Eigen::Affine3f Transformation;
	Transformation transformation;
	Eigen::Vector3f size;
};

// z: 9, x: 9, y: 8
const Eigen::Vector3i neighborOffsets[] = {
	// first z, then x, then y

	// z
	Eigen::Vector3i( 0, 0, 1 ), Eigen::Vector3i( 0, 0, -1 ), Eigen::Vector3i( -1, 0, -1 ),
	Eigen::Vector3i( 1, 0, -1 ), Eigen::Vector3i( -1, 0, 1 ),  Eigen::Vector3i( 1, 0, 1 ),
	Eigen::Vector3i( 0, 1, -1 ), Eigen::Vector3i( 0, -1, -1 ), Eigen::Vector3i( -1, 1, -1 ),
	// x
	Eigen::Vector3i( 1, 0, 0 ), Eigen::Vector3i( -1, 0, 0  ), Eigen::Vector3i( 1, 1, -1 ),
	Eigen::Vector3i( 1, -1, 1 ), Eigen::Vector3i( 1, 1, 1 ), Eigen::Vector3i( 1, -1, -1 ),
	Eigen::Vector3i( -1, 1, 0 ), Eigen::Vector3i( 1, 1, 0 ), Eigen::Vector3i( -1, -1, -1 ),
	// y
	Eigen::Vector3i( 0, 1, 0 ), Eigen::Vector3i( 0, -1, 0 ), Eigen::Vector3i( -1, -1, 0 ), Eigen::Vector3i( 1, -1, 0 ),
	Eigen::Vector3i( -1, -1, 1 ), Eigen::Vector3i( 0, -1, 1 ),  Eigen::Vector3i( -1, 1, 1 ), Eigen::Vector3i( 0, 1, 1 )
};

inline Eigen::Vector3i ceil( const Eigen::Vector3f &v );
inline Eigen::Vector3i floor( const Eigen::Vector3f &v );

template< typename Vector > Vector permute( const Vector &v, const int *permutation );
template< typename Vector > Vector permute_reverse( const Vector &w, const int *permutation );
inline Eigen::Matrix4f permutedToUnpermutedMatrix( const int *permutation );
inline Eigen::Matrix4f unpermutedToPermutedMatrix( const int *permutation );

OBB makeOBB( const Eigen::Matrix4f &transformation, const Eigen::AlignedBox3f &alignedBox );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// implementation
// 
inline Eigen::Vector3i ceil( const Eigen::Vector3f &v ) {
	return Eigen::Vector3i( (int) ceil( v[0] ), (int) ceil( v[1] ), (int) ceil( v[2] ) );
}

inline Eigen::Vector3i floor( const Eigen::Vector3f &v ) {
	return Eigen::Vector3i( (int) floor( v[0] ), (int) floor( v[1] ), (int) floor( v[2] ) );
}

// xyz -120-> yzx
template< typename Vector >
Vector permute( const Vector &v, const int *permutation ) {
	return Vector( v[permutation[0]], v[permutation[1]], v[permutation[2]] );
}

// yzx -120->xyz
template< typename Vector >
Vector permute_reverse( const Vector &w, const int *permutation ) {
	Vector v;
	for( int i = 0 ; i < 3 ; ++i ) {
		v[ permutation[i] ] = w[i];	
	}
	return v;
}

inline Eigen::Matrix4f permutedToUnpermutedMatrix( const int *permutation ) {
	return (Eigen::Matrix4f() << Eigen::Vector3f::Unit( permutation[0] ), Eigen::Vector3f::Unit( permutation[1] ), Eigen::Vector3f::Unit( permutation[2] ), Eigen::Vector3f::Zero(), 0,0,0,1.0 ).finished();
}

inline Eigen::Matrix4f unpermutedToPermutedMatrix( const int *permutation ) {
	return (Eigen::Matrix4f() << Eigen::RowVector3f::Unit( permutation[0] ), 0.0,
		Eigen::RowVector3f::Unit( permutation[1] ), 0.0,
		Eigen::RowVector3f::Unit( permutation[2] ), 0.0,
		Eigen::RowVector4f::UnitW() ).finished();
}

inline OBB makeOBB( const Eigen::Matrix4f &transformation, const Eigen::AlignedBox3f &alignedBox ) {
	OBB obb;
	obb.transformation = Eigen::Affine3f( transformation ) * Eigen::Translation3f( alignedBox.center() );
	obb.size = alignedBox.sizes();
	return obb;
}
