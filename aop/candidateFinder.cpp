#include "candidateFinderInterface.h"

void ProbeDataset::sort() {
	AUTO_TIMER_FOR_FUNCTION();

	using namespace generic;

	const auto iterator_begin = make_sort_permute_iter( probeContexts.begin(), probes.begin() );
	const auto iterator_end = make_sort_permute_iter( probeContexts.end(), probes.end() );

	std::sort(
		iterator_begin,
		iterator_end,
		make_sort_permute_iter_compare_pred<decltype(iterator_begin)>( probeContext_lexicographicalLess )
	);
}

void ProbeDataset::setHitCounterLowerBounds() {
	AUTO_TIMER_FOR_FUNCTION();

	// improvement idea: use a binary search like algorithm

	// 0..numProbeSamples are valid hitCounter values
	// we store one additional end() lower bound for simple interval calculations
	hitCounterLowerBounds.reserve( OptixProgramInterface::numProbeSamples + 2 );
	hitCounterLowerBounds.clear();

	int level  = 0;

	// begin with level 0
	hitCounterLowerBounds.push_back( 0 );

	for( int probeIndex = 0 ; probeIndex < size() ; ++probeIndex ) {
		const auto current = probeContexts[probeIndex].hitCounter;
		for( ; level < current ; level++ ) {
			hitCounterLowerBounds.push_back( probeIndex );
		}
	}

	// TODO: store min and max level for simpler early out?
	// fill the remaining levels
	for( ; level <= OptixProgramInterface::numProbeSamples ; level++ ) {
		hitCounterLowerBounds.push_back( size() );
	}
}

ProbeDataset ProbeDataset::merge( const ProbeDataset &first, const ProbeDataset &second ) {
	AUTO_TIMER_DEFAULT( boost::str( boost::format( "with %i + %i = %i probes ") % first.size() % second.size() % (first.size() + second.size()) ) );

	using namespace generic;

	ProbeDataset result;
	result.probes.resize( first.probes.size() + second.probes.size() );
	result.probeContexts.resize( first.probeContexts.size() + second.probeContexts.size() );

	const auto first_iterator_begin = make_sort_permute_iter( first.probeContexts.begin(), first.probes.begin() );
	const auto first_iterator_end = make_sort_permute_iter( first.probeContexts.end(), first.probes.end() );

	const auto second_iterator_begin = make_sort_permute_iter( second.probeContexts.begin(), second.probes.begin() );
	const auto second_iterator_end = make_sort_permute_iter( second.probeContexts.end(), second.probes.end() );

	const auto result_iterator_begin = make_sort_permute_iter( result.probeContexts.begin(), result.probes.begin() );

	std::merge(
		first_iterator_begin,
		first_iterator_end,
		second_iterator_begin,
		second_iterator_end,
		result_iterator_begin,
		make_sort_permute_iter_compare_pred<decltype(first_iterator_begin)>( probeContext_lexicographicalLess )
	);

	return std::move( result );
}

ProbeDataset ProbeDataset::mergeMultiple( const std::vector< ProbeDataset* > &datasets) {
	AUTO_TIMER_DEFAULT( boost::str( boost::format( "with %i datasets" ) % datasets.size() ) );

	using namespace generic;

	// best performance when using binary merges:
	//	use a priority queue to merge the two shortest datasets into a bigger one
	//	reinsert the resulting dataset into the heap
	//
	// use in-place merges, so no additional memory has to be allocated
	//
	// different idea:
	//	implement n-way merge using a priority queue that keeps track where the elements were added from
	//	for every element that drops out front, a new element from the dataset is inserted (until it runs out of elements)

	if( datasets.size() < 2 ) {
		throw std::invalid_argument( "less than 2 datasets passed to mergeMultiple!" );
	}

	// determine the total count of all probes
	int totalCount = 0;
	for( auto dataset = datasets.begin() ; dataset != datasets.end() ; ++dataset ) {
		totalCount += (int) (*dataset)->probes.size();
	}

	std::cerr << autoTimer.indentation() << "merging " << totalCount << " probes\n";

	ProbeDataset result;

	// reserve enough space for all probes
	result.probes.reserve( totalCount );
	result.probeContexts.reserve( totalCount );


	for( auto dataset = datasets.begin() ; dataset != datasets.end() ; ++dataset ) {
		boost::push_back( result.probes, (*dataset)->probes );
		boost::push_back( result.probeContexts, (*dataset)->probeContexts );
	}

	// stable sorting everything is the next best alternative to doing it manually :)
	{
		const auto iterator_begin = make_sort_permute_iter( result.probeContexts.begin(), result.probes.begin() );
		const auto iterator_end = make_sort_permute_iter( result.probeContexts.end(), result.probes.end() );

		std::stable_sort(
			iterator_begin,
			iterator_end,
			make_sort_permute_iter_compare_pred<decltype(iterator_begin)>( probeContext_lexicographicalLess )
		);
	}

	return std::move( result );
}