#include "probeDatabase.h"
#include "boost/range/algorithm/merge.hpp"

#if 0
InstanceProbeDataset InstanceProbeDataset::merge( const InstanceProbeDataset &first, const InstanceProbeDataset &second ) {
	AUTO_TIMER_DEFAULT( boost::str( boost::format( "with %i + %i = %i probes ") % first.size() % second.size() % (first.size() + second.size()) ) );

	InstanceProbeDataset dataset;
	dataset.probeSamples().resize( first.size() + second.size() );
	boost::merge( first.getProbeSamples(), second.getProbeSamples(), dataset.probeSamples().begin(), ProbeSample::lexicographicalLess );

	return std::move( dataset );
}

InstanceProbeDataset InstanceProbeDataset::mergeMultiple( const std::vector< const InstanceProbeDataset* > &datasets) {
	AUTO_TIMER_DEFAULT( boost::str( boost::format( "with %i datasets" ) % datasets.size() ) );

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

	log( boost::format( "merging %i probes" ) % totalCount );

	InstanceProbeDataset mergedDataset;

	// reserve enough space for all probes
	mergedDataset.probeSamples().reserve( totalCount );

	for( auto dataset = datasets.begin() ; dataset != datasets.end() ; ++dataset ) {
		boost::push_back( mergedDataset.probeSamples(), (*dataset)->getProbeSamples() );
	}

	// stable sorting everything is the next best alternative to doing it manually :)
	boost::stable_sort( mergedDataset.probeSamples(), ProbeSample::lexicographicalLess );

	return std::move( mergedDataset );
}

InstanceProbeDataset InstanceProbeDataset::subSet( const std::pair< int, int > &range ) const {

	InstanceProbeDataset dataset;

	const int rangeSize = range.second - range.first;
	if( !rangeSize ) {
		return dataset;
	}

	dataset.probeSamples().resize( rangeSize );

	// copy data
	std::copy( getProbeSamples().begin() + range.first, getProbeSamples().begin() + range.second, dataset.probeSamples().begin() );

	// TODO: either use sort or stable_sort depending on the size of the dataset
	boost::stable_sort( dataset.probeSamples(), ProbeSample::lexicographicalLess_startWithDistance );

	return dataset;
}
#endif

void IndexedProbeSamples::setHitCounterLowerBounds() {
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
		const auto current = getProbeSamples()[probeIndex].hitCounter;
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

void ProbeDatabase::registerSceneModels( const std::vector< std::string > &modelNames ) {
	modelIndexMapper.registerSceneModels( modelNames );
	modelIndexMapper.registerLocalModels( localModelNames );
}

void ProbeDatabase::clearAll() {
	sampledModels.clear();
	localModelNames.clear();
	modelIndexMapper.resetLocalMaps();
}

void ProbeDatabase::clear( int sceneModelIndex ) {
	const int localModelIndex = modelIndexMapper.getLocalModelIndex( sceneModelIndex );
	if( localModelIndex != ModelIndexMapper::INVALID_INDEX ) {
		sampledModels.erase( sampledModels.begin() + localModelIndex );
	}
	modelIndexMapper.registerLocalModels( localModelNames );
}

void ProbeDatabase::addInstanceProbes( int sceneModelIndex, const Obb &sampleSource, const RawProbes &untransformedProbes, const RawProbeSamples &probeSamples ) {
	int localModelIndex = modelIndexMapper.getLocalModelIndex( sceneModelIndex );
	if( localModelIndex == ModelIndexMapper::INVALID_INDEX ) {
		localModelIndex = localModelNames.size();
		localModelNames.push_back( modelIndexMapper.getSceneModelName( sceneModelIndex ) );
		modelIndexMapper.registerLocalModel( localModelNames.back() );

		sampledModels.emplace_back( SampledModel() );
	}

	SampledModel &idDataset = sampledModels[ localModelIndex ];
	idDataset.addInstances( untransformedProbes, sampleSource, ProbeSampleTransformation::transformSamples( probeSamples ) );
}

void ProbeDatabase::compile( int sceneModelIndex ) {
	const int localModelIndex = modelIndexMapper.getLocalModelIndex( sceneModelIndex );
	if( localModelIndex != ModelIndexMapper::INVALID_INDEX ) {
		sampledModels[ localModelIndex ].mergeInstances();
	}
}
