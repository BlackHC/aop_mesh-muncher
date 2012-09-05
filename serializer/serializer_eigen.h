#pragma once

#include "serializer.h"

#include <Eigen/Eigen>

namespace Serializer {
	namespace detail {
		template<typename T> 
		struct EigenMatrixTraits {
		};

		template< typename _Scalar, int _Rows, int _Cols >
		struct EigenMatrixTraits< Eigen::Matrix< _Scalar, _Rows, _Cols > > {
			typedef void isEigenType;

			typedef _Scalar (*ArrayPointer)[ _Rows * _Cols ];
		};

		template<typename T>
		struct EigenAlignedBox {
		};

		template< typename _Scalar, int _Dim >
		struct EigenAlignedBox< Eigen::AlignedBox< _Scalar, _Dim > > {
			typedef void isEigenType;
		};
	}

	template< typename Reader, typename X >
	typename detail::EigenMatrixTraits<X>::isEigenType read( Reader &reader, X &value ) {
		typedef detail::EigenMatrixTraits<X>::ArrayPointer ArrayPointer;
		ArrayPointer array = (ArrayPointer) value.data();
		read( reader, *array );
	}

	template< typename Writer, typename X >
	typename detail::EigenMatrixTraits<X>::isEigenType write( Writer &writer, const X &value ) {
		typedef detail::EigenMatrixTraits<X>::ArrayPointer ArrayPointer;
		ArrayPointer array = (ArrayPointer) value.data();
		write( writer, *array );
	}

// TODO: add error/warning
#ifdef min 
#	undef min
#endif
#ifdef max
#	undef max
#endif
	
	template< typename Reader, typename X >
	typename detail::EigenAlignedBox<X>::isEigenType read( Reader &reader, X &value ) {
		get( reader, "min", value.min() );
		get( reader, "max", value.max() );
	}

	template< typename Writer, typename X >
	typename detail::EigenAlignedBox<X>::isEigenType write( Writer &writer, const X &value ) {
		put( writer, "min", value.min() );
		put( writer, "max", value.max() );
	}
}