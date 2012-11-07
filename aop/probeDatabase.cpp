#include "probeDatabase.h"
#include "boost/range/algorithm/merge.hpp"

namespace ProbeContext {
void IndexedProbeSamples::setOcclusionLowerBounds() {
	AUTO_TIMER_FOR_FUNCTION();

	// improvement idea: use a binary search like algorithm

	// 0..numProbeSamples are valid occlusion values
	// we store one additional end() lower bound for simple interval calculations
	occlusionLowerBounds.reserve( OptixProgramInterface::numProbeSamples + 2 );
	occlusionLowerBounds.clear();

	int level  = 0;

	// begin with level 0
	occlusionLowerBounds.push_back( 0 );

	for( int probeIndex = 0 ; probeIndex < size() ; ++probeIndex ) {
		const auto current = getProbeSamples()[probeIndex].occlusion;
		for( ; level < current ; level++ ) {
			occlusionLowerBounds.push_back( probeIndex );
		}
	}

	// TODO: store min and max level for simpler early out?
	// fill the remaining levels
	for( ; level <= OptixProgramInterface::numProbeSamples ; level++ ) {
		occlusionLowerBounds.push_back( size() );
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

void ProbeDatabase::addInstanceProbes(
	int sceneModelIndex,
	const Obb::Transformation &sourceTransformation,
	const float resolution,
	const RawProbes &probes,
	const RawProbeSamples &probeSamples
) {
	int localModelIndex = modelIndexMapper.getLocalModelIndex( sceneModelIndex );
	if( localModelIndex == ModelIndexMapper::INVALID_INDEX ) {
		localModelIndex = localModelNames.size();
		localModelNames.push_back( modelIndexMapper.getSceneModelName( sceneModelIndex ) );
		modelIndexMapper.registerLocalModel( localModelNames.back() );

		sampledModels.emplace_back( SampledModel() );
	}

	SampledModel &sampledModel = sampledModels[ localModelIndex ];
	sampledModel.addInstanceProbes( sourceTransformation, resolution, probes, ProbeSampleTransformation::transformSamples( probeSamples ) );
}

void ProbeDatabase::compile( int sceneModelIndex ) {
	const int localModelIndex = modelIndexMapper.getLocalModelIndex( sceneModelIndex );
	if( localModelIndex != ModelIndexMapper::INVALID_INDEX ) {
		sampledModels[ localModelIndex ].mergeInstances( colorCounter );
	}
}
}
