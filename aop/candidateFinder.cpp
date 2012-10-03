#include "candidateFinderInterface.h"

void RawProbeDataset::sort() {
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

SortedProbeDataset SortedProbeDataset::merge( const SortedProbeDataset &first, const SortedProbeDataset &second ) {
	AUTO_TIMER_DEFAULT( boost::str( boost::format( "with %i + %i = %i probes ") % first.size() % second.size() % (first.size() + second.size()) ) );

	using namespace generic;

	SortedProbeDataset dataset;
	dataset.probes().resize( first.size() + second.size() );
	dataset.probeContexts().resize( first.size() + second.size() );

	const auto first_iterator_begin = make_sort_permute_iter( first.getProbeContexts().begin(), first.getProbes().begin() );
	const auto first_iterator_end = make_sort_permute_iter( first.getProbeContexts().end(), first.getProbes().end() );

	const auto second_iterator_begin = make_sort_permute_iter( second.getProbeContexts().begin(), second.getProbes().begin() );
	const auto second_iterator_end = make_sort_permute_iter( second.getProbeContexts().end(), second.getProbes().end() );

	const auto dataset_iterator_begin = make_sort_permute_iter( dataset.probeContexts().begin(), dataset.probes().begin() );

	std::merge(
		first_iterator_begin,
		first_iterator_end,
		second_iterator_begin,
		second_iterator_end,
		dataset_iterator_begin,
		make_sort_permute_iter_compare_pred<decltype(first_iterator_begin)>( probeContext_lexicographicalLess )
	);

	return std::move( dataset );
}

SortedProbeDataset SortedProbeDataset::mergeMultiple( const std::vector< const SortedProbeDataset* > &datasets) {
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
		totalCount += (int) (*dataset)->size();
	}

	std::cerr << autoTimer.indentation() << "merging " << totalCount << " probes\n";

	SortedProbeDataset mergedDataset;

	// reserve enough space for all probes
	mergedDataset.probes().reserve( totalCount );
	mergedDataset.probeContexts().reserve( totalCount );


	for( auto dataset = datasets.begin() ; dataset != datasets.end() ; ++dataset ) {
		boost::push_back( mergedDataset.probes(), (*dataset)->getProbes() );
		boost::push_back( mergedDataset.probeContexts(), (*dataset)->getProbeContexts() );
	}

	// stable sorting everything is the next best alternative to doing it manually :)
	{
		const auto iterator_begin = make_sort_permute_iter( mergedDataset.probeContexts().begin(), mergedDataset.probes().begin() );
		const auto iterator_end = make_sort_permute_iter( mergedDataset.probeContexts().end(), mergedDataset.probes().end() );

		std::stable_sort(
			iterator_begin,
			iterator_end,
			make_sort_permute_iter_compare_pred<decltype(iterator_begin)>( probeContext_lexicographicalLess )
		);
	}

	return std::move( mergedDataset );
}

SortedProbeDataset SortedProbeDataset::subSet( const std::pair< int, int > &range ) const {
	using namespace generic;

	SortedProbeDataset dataset;

	const int rangeSize = range.second - range.first;
	if( !rangeSize ) {
		return dataset;
	}

	dataset.probes().reserve( rangeSize );
	dataset.probeContexts().reserve( rangeSize );

	// copy data
	std::copy( getProbeContexts().begin() + range.first, getProbeContexts().begin() + range.second, std::back_inserter( dataset.probeContexts() ) );
	std::copy( getProbes().begin() + range.first, getProbes().begin() + range.second, std::back_inserter( dataset.probes() ) );

	auto iterator_begin = make_sort_permute_iter( dataset.probeContexts().begin(), dataset.probes().begin() );
	auto iterator_end = make_sort_permute_iter( dataset.probeContexts().end(), dataset.probes().end() );

	// TODO: either use sort or stable_sort depending on the size of the dataset
	std::stable_sort( iterator_begin, iterator_end, make_sort_permute_iter_compare_pred< decltype( iterator_begin ) >( probeContext_lexicographicalLess_startWithDistance ) );

	return dataset;
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
		const auto current = getProbeContexts()[probeIndex].hitCounter;
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