#pragma once

#include "serializer.h"

#include <Eigen/Eigen>

namespace Serializer {
	template< typename Reader, typename Scalar >
	void read( Reader &reader, Eigen::Matrix< Scalar, 3, 1 > &value ) {
		typedef Scalar (*ArrayPointer)[3];
		ArrayPointer array = (ArrayPointer) &value[0];
		read( reader, *array );
	}

	template< typename Emitter, typename Scalar >
	void write( Emitter &emitter, const Eigen::Matrix<Scalar, 3, 1> &value ) {
		typedef const Scalar (*ArrayPointer)[3];
		ArrayPointer array = (ArrayPointer) &value[0];
		write( emitter, *array );
	}

	template< typename Reader >
	void read( Reader &reader, Eigen::AlignedBox3f &value ) {
		get( reader, "min", value.min() );
		get( reader, "max", value.max() );
	}

	template< typename Emitter >
	void write( Emitter &emitter, const Eigen::AlignedBox3f &value ) {
		put( emitter, "min", value.min() );
		put( emitter, "max", value.max() );
	}

	// instantiate for Vector2f
	template void write< TextEmitter, float >( TextEmitter &, const Eigen::Matrix< float, 2, 1 > & );
	// instantiate for Vector3f
	template void write< TextEmitter, float >( TextEmitter &, const Eigen::Matrix< float, 3, 1 > & );
	// instantiate for Vector4f
	template void write< TextEmitter, float >( TextEmitter &, const Eigen::Matrix< float, 4, 1 > & );
}