#pragma once

#include "boost/iterator/iterator_facade.hpp"
#include "boost/tuple/tuple.hpp"
#include <boost/type_traits.hpp>
#include <functional>

// based on http://www.stanford.edu/~dgleich/notebook/2006/03/sorting_two_arrays_simultaneou.html
namespace generic {
	namespace detail {
		template <class SortIter, class PermuteIter>
		struct sort_permute_iter_helper_type
		{
			typedef boost::tuple<
				typename std::iterator_traits<SortIter>::value_type,
				typename std::iterator_traits<PermuteIter>::value_type >
				value_type;

			typedef boost::tuple<
				typename std::iterator_traits<SortIter>::reference,
				typename std::iterator_traits<PermuteIter>::reference >
				ref_type;
		};
	}

	template <class SortIter, class PermuteIter>
	class sort_permute_iter
		: public boost::iterator_facade<
		sort_permute_iter<SortIter, PermuteIter>,
		typename detail::sort_permute_iter_helper_type<SortIter, PermuteIter>::value_type,
		std::random_access_iterator_tag,
		typename detail::sort_permute_iter_helper_type<SortIter, PermuteIter>::ref_type,
		typename std::iterator_traits<SortIter>::difference_type
		>
	{
	public:
		typedef SortIter sort_iterator;
		typedef PermuteIter permute_iterator;

		sort_permute_iter()
		{}

		sort_permute_iter(SortIter ci, PermuteIter vi)
			: _ci(ci), _vi(vi)
		{
		}

		SortIter _ci;
		PermuteIter _vi;


	private:
		friend class boost::iterator_core_access;

		void increment()
		{
			++_ci; ++_vi;
		}

		void decrement()
		{
			--_ci; --_vi;
		}

		bool equal(sort_permute_iter const& other) const
		{
			return (_ci == other._ci);
		}

		typename
			detail::sort_permute_iter_helper_type<
			SortIter, PermuteIter>::ref_type dereference() const
		{
			return (typename detail::sort_permute_iter_helper_type<SortIter, PermuteIter>::ref_type(*_ci, *_vi));
		}

		void advance(difference_type n)
		{
			_ci += n;
			_vi += n;
		}

		difference_type distance_to(sort_permute_iter const& other) const
		{
			return ( other._ci - _ci);
		}
	};


	template <class SortIter, class PermuteIter>
	struct sort_permute_iter_compare
		: public std::binary_function<
		typename detail::sort_permute_iter_helper_type<SortIter, PermuteIter>::value_type,
		typename detail::sort_permute_iter_helper_type<SortIter, PermuteIter>::value_type,
		bool
		>
	{
		typedef
			typename detail::sort_permute_iter_helper_type<
			SortIter, PermuteIter>::value_type T;
		bool operator()(const  T& t1, const T& t2)
		{
			return (boost::get<0>(t1) < boost::get<0>(t2));
		}
	};

	template <typename Pred, class SortIter, class PermuteIter>
	struct sort_permute_iter_compare_pred
		: public std::binary_function<
		typename detail::sort_permute_iter_helper_type<SortIter, PermuteIter>::value_type,
		typename detail::sort_permute_iter_helper_type<SortIter, PermuteIter>::value_type,
		bool
		>
	{
		typedef
			typename detail::sort_permute_iter_helper_type<
			SortIter, PermuteIter>::value_type T;

		typedef Pred Pred;

		Pred pred;

		sort_permute_iter_compare_pred() {}
		sort_permute_iter_compare_pred( Pred pred ) : pred( pred ) {}

		bool operator()(const  T& t1, const T& t2)
		{
			return pred(boost::get<0>(t1), boost::get<0>(t2));
		}
	};

	template <class SortIter, class PermuteIter>
	sort_permute_iter<SortIter, PermuteIter>
		make_sort_permute_iter(const SortIter &ci, const PermuteIter &vi)
	{
		return sort_permute_iter<SortIter, PermuteIter>(ci, vi);
	}

	template <class SortIter, class PermuteIter>
	sort_permute_iter<SortIter, PermuteIter>
		make_sort_permute_iter_compare(SortIter ci, PermuteIter vi)
	{
		return sort_permute_iter_compare<SortIter, PermuteIter>();
	}

	template <class SortIter, class PermuteIter>
	sort_permute_iter_compare<SortIter, PermuteIter>
		make_sort_permute_iter_compare( const sort_permute_iter<SortIter, PermuteIter> & )
	{
		return sort_permute_iter_compare<SortIter, PermuteIter>();
	}

	namespace detail {
		template <typename T>
		struct iterator_trait {
		};

		template <class _SortIter, class _PermuteIter>
		struct iterator_trait< sort_permute_iter<_SortIter, _PermuteIter> > {
			typedef typename boost::decay< typename boost::remove_cv< _SortIter >::type >::type SortIter;
			typedef typename boost::decay< typename boost::remove_cv< _PermuteIter >::type >::type PermuteIter;
		};
	}

	template <typename T>
	sort_permute_iter_compare< typename T::sort_iterator, typename T::permute_iterator >
		make_sort_permute_iter_compare()
	{
		return sort_permute_iter_compare< typename T::sort_iterator, typename T::permute_iterator >();
	}

	template <typename T, typename Pred>
	sort_permute_iter_compare_pred< Pred, typename T::sort_iterator, typename T::permute_iterator >
		make_sort_permute_iter_compare_pred( Pred pred)
	{
		return sort_permute_iter_compare_pred< Pred, typename T::sort_iterator, typename T::permute_iterator >( pred );
	}
}


#ifdef TEST_GENERIC_SORT_PERMUTE_ITER
#include "gtest.h"

using namespace generic;

TEST(generic_sort_permute_iter, int_int) {
	std::vector<int> a, b;

	for( int i = 0 ; i < 1000 ; i++ ) {
		a.push_back( -i );
		b.push_back( -i );
	}

	auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare( iterator_begin ) );

	for( int i = 0, j = 1000 - 1 ; i < 1000 ; i++, j-- ) {
		ASSERT_EQ( -j, a[i] );
		ASSERT_EQ( -j, b[i] );
	}
}

TEST(generic_sort_permute_iter, int_int_const_auto ) {
	std::vector<int> a, b;

	for( int i = 0 ; i < 1000 ; i++ ) {
		a.push_back( -i );
		b.push_back( -i );
	}

	const auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	const auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare( iterator_begin ) );

	for( int i = 0, j = 1000 - 1 ; i < 1000 ; i++, j-- ) {
		ASSERT_EQ( -j, a[i] );
		ASSERT_EQ( -j, b[i] );
	}
}

TEST(generic_sort_permute_iter, int_int_decltype) {
	std::vector<int> a, b;

	for( int i = 0 ; i < 1000 ; i++ ) {
		a.push_back( -i );
		b.push_back( -i );
	}

	auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare< decltype(iterator_begin) >() );

	for( int i = 0, j = 1000 - 1 ; i < 1000 ; i++, j-- ) {
		ASSERT_EQ( -j, a[i] );
		ASSERT_EQ( -j, b[i] );
	}
}

TEST(generic_sort_permute_iter, int_int_decltype_const_auto) {
	std::vector<int> a, b;

	for( int i = 0 ; i < 1000 ; i++ ) {
		a.push_back( -i );
		b.push_back( -i );
	}

	const auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	const auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare< decltype(iterator_begin) >() );

	for( int i = 0, j = 1000 - 1 ; i < 1000 ; i++, j-- ) {
		ASSERT_EQ( -j, a[i] );
		ASSERT_EQ( -j, b[i] );
	}
}

TEST(generic_sort_permute_iter, int_int_greater) {
	std::vector<int> a, b;

	for( int i = 0 ; i < 1000 ; i++ ) {
		a.push_back( i );
		b.push_back( i );
	}

	auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare_pred< decltype( iterator_begin ) >( [] (int x, int y) { return x > y; } ) );

	for( int i = 0, j = 1000 - 1 ; i < 1000 ; i++, j-- ) {
		ASSERT_EQ( j, a[i] );
		ASSERT_EQ( j, b[i] );
	}
}

TEST(generic_sort_permute_iter, int_int_greater_const_auto) {
	std::vector<int> a, b;

	for( int i = 0 ; i < 1000 ; i++ ) {
		a.push_back( i );
		b.push_back( i );
	}

	const auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	const auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare_pred< decltype( iterator_begin ) >( [] (int x, int y) { return x > y; } ) );

	for( int i = 0, j = 1000 - 1 ; i < 1000 ; i++, j-- ) {
		ASSERT_EQ( j, a[i] );
		ASSERT_EQ( j, b[i] );
	}
}

#include <boost/lexical_cast.hpp>

TEST(generic_sort_permute_iter, int_string_greater) {
	std::vector<int> a;
	std::vector<std::string> b;

	for( int i = 0 ; i < 1000 ; i++ ) {
		a.push_back( i );
		b.push_back( boost::lexical_cast<std::string>(i) );
	}

	auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare_pred< decltype( iterator_begin )>( [] (int x, int y) { return x > y; } ) );

	for( int i = 0, j = 1000 - 1 ; i < 1000 ; i++, j-- ) {
		ASSERT_EQ( j, a[i] );
		ASSERT_EQ( j, boost::lexical_cast<int>( b[i] ) );
	}
}

// std::merge

TEST(generic_sort_permute_iter, int_int_merge) {
	std::vector<int> a[2], b[2];

	for( int i = 0 ; i < 1000 ; i++ ) {
		a[0].push_back( 2*i ); a[1].push_back( 2*i + 1 );
		b[0].push_back( 2*i ); b[1].push_back( 2*i + 1 );
	}

	typedef decltype(make_sort_permute_iter( a[0].begin(), b[0].begin() )) iterator;
	iterator iterator_begin[2] = { make_sort_permute_iter( a[0].begin(), b[0].begin() ), make_sort_permute_iter( a[1].begin(), b[1].begin() ) };
	iterator iterator_end[2] = { make_sort_permute_iter( a[0].end(), b[0].end() ), make_sort_permute_iter( a[1].end(), b[1].end() ) };

	std::vector<int> a_out, b_out;

	a_out.resize(2000);
	b_out.resize(2000);

	iterator out_iterator = make_sort_permute_iter( a_out.begin(), b_out.begin() );
	std::merge( iterator_begin[0], iterator_end[0], iterator_begin[1], iterator_end[1], out_iterator, make_sort_permute_iter_compare( iterator_begin[0] ) );



	for( int i = 0 ;  i < 2000 ; i++ ) {
		ASSERT_EQ( i, a_out[i] );
		ASSERT_EQ( i, b_out[i] );
	}
}

TEST(generic_sort_permute_iter, const_int_int_merge) {
	std::vector<int> a[2], b[2];

	for( int i = 0 ; i < 1000 ; i++ ) {
		a[0].push_back( 2*i ); a[1].push_back( 2*i + 1 );
		b[0].push_back( 2*i ); b[1].push_back( 2*i + 1 );
	}

	const std::vector<int> &ca0 = a[0];
	const std::vector<int> &ca1 = a[1];
	const std::vector<int> &cb0 = b[0];
	const std::vector<int> &cb1 = b[1];

	typedef decltype(make_sort_permute_iter( ca0.begin(), cb0.begin() )) iterator;
	iterator iterator_begin[2] = { make_sort_permute_iter( ca0.begin(), cb0.begin() ), make_sort_permute_iter( ca1.begin(), cb1.begin() ) };
	iterator iterator_end[2] = { make_sort_permute_iter( ca0.end(), cb0.end() ), make_sort_permute_iter( ca1.end(), cb1.end() ) };

	std::vector<int> a_out, b_out;

	a_out.resize(2000);
	b_out.resize(2000);

	auto out_iterator = make_sort_permute_iter( a_out.begin(), b_out.begin() );
	std::merge( iterator_begin[0], iterator_end[0], iterator_begin[1], iterator_end[1], out_iterator, make_sort_permute_iter_compare( iterator_begin[0] ) );



	for( int i = 0 ;  i < 2000 ; i++ ) {
		ASSERT_EQ( i, a_out[i] );
		ASSERT_EQ( i, b_out[i] );
	}
}

TEST(generic_sort_permute_iter, int_int_inplace_merge ) {
	std::vector<int> a, b;

	for( int i = 0 ; i < 500 ; i++ ) {
		a.push_back( 2*i );
		b.push_back( 2*i );
	}

	for( int i = 0 ; i < 500 ; i++ ) {
		a.push_back( 2*i + 1 );
		b.push_back( 2*i + 1 );
	}

	auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	auto iterator_middle = make_sort_permute_iter( a.begin() + 500, b.begin() + 500 );
	auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::inplace_merge( iterator_begin, iterator_middle, iterator_end, make_sort_permute_iter_compare( iterator_begin ) );

	for( int i = 0 ; i < 1000 ; i++ ) {
		ASSERT_EQ( i, a[i] );
		ASSERT_EQ( i, b[i] );
	}
}


TEST(generic_sort_permute_iter, int_int_stable_sort) {
	std::vector<int> a, b;

	for( int i = 0 ; i < 1000 ; i++ ) {
		a.push_back( -i );
		b.push_back( -i );
	}

	auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
	auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

	std::stable_sort( iterator_begin, iterator_end, make_sort_permute_iter_compare( iterator_begin ) );

	for( int i = 0, j = 1000 - 1 ; i < 1000 ; i++, j-- ) {
		ASSERT_EQ( -j, a[i] );
		ASSERT_EQ( -j, b[i] );
	}
}

TEST(generic_sort_permute_iter, sort_performance) {
	std::vector<int> a, b;
	a.reserve( 1000 );
	b.reserve( 1000 );

	for( int j = 0 ; j < 100 ; j++ ) {
		for( int i = 0 ; i < 1000 ; i++ ) {
			a.push_back( -i );
			b.push_back( -i );
		}

		auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
		auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

		std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare( iterator_begin ) );
	}
}

TEST(generic_sort_permute_iter, stable_sort_performance) {
	std::vector<int> a, b;
	a.reserve( 1000 );
	b.reserve( 1000 );

	for( int j = 0 ; j < 100 ; j++ ) {
		for( int i = 0 ; i < 1000 ; i++ ) {
			a.push_back( -i );
			b.push_back( -i );
		}

		auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
		auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

		std::stable_sort( iterator_begin, iterator_end, make_sort_permute_iter_compare( iterator_begin ) );
	}
}

#if 0
TEST(generic_sort_permute_iter, sort_performance_half_sorted) {
	std::vector<int> a, b;
	a.reserve( 1000000 );
	b.reserve( 1000000 );

	for( int j = 0 ; j < 10 ; j++ ) {
		for( int i = 0 ; i < 500000 ; i++ ) {
			a.push_back( 2*i );
			b.push_back( 2*i );
		}

		for( int i = 0 ; i < 500000 ; i++ ) {
			a.push_back( 1 + 2*i );
			b.push_back( 1 + 2*i );
		}

		auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
		auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

		std::sort( iterator_begin, iterator_end, make_sort_permute_iter_compare( iterator_begin ) );
	}
}

TEST(generic_sort_permute_iter, stable_sort_performance_half_sorted) {
	std::vector<int> a, b;
	a.reserve( 1000000 );
	b.reserve( 1000000 );

	for( int j = 0 ; j < 10 ; j++ ) {
		for( int i = 0 ; i < 500000 ; i++ ) {
			a.push_back( 2*i );
			b.push_back( 2*i );
		}

		for( int i = 0 ; i < 500000 ; i++ ) {
			a.push_back( 1 + 2*i );
			b.push_back( 1 + 2*i );
		}

		auto iterator_begin = make_sort_permute_iter( a.begin(), b.begin() );
		auto iterator_end = make_sort_permute_iter( a.end(), b.end() );

		std::stable_sort( iterator_begin, iterator_end, make_sort_permute_iter_compare( iterator_begin ) );
	}
}
#endif

#endif