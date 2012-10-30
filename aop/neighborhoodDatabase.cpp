#include "neighborhoodDatabase.h"

NeighborhoodDatabase::NeighborhoodContext::NeighborhoodContext( RawIdDistances &&rawDataset ) {
	boost::sort( rawDataset );

	for( auto idDistancePair = rawDataset.begin() ; idDistancePair != rawDataset.end() ; ) {
		int currentId = idDistancePair->first;

		distancesById.push_back( IdDistancesPair() );
		auto &idDistancesPair = distancesById.back();
		idDistancesPair.first = currentId;

		for( ; idDistancePair != rawDataset.end() && idDistancePair->first == currentId ; ++idDistancePair ) {
			idDistancesPair.second.push_back( idDistancePair->second );
		}
	}
}

NeighborhoodDatabase::Dataset::Dataset( float binWidth, float maxDistance, const NeighborhoodContext &sortedDataset )
	: binWidth( binWidth )
	, maxDistance( maxDistance )
{
	const float halfBinWidth = binWidth / 2;
	const int numBins = getNumBins( binWidth, maxDistance );

	const int numIds = sortedDataset.getNumIds();
	binsById.resize( numIds );
	for( int i = 0 ; i < numIds ; i++ ) {
		const auto &idDistancePair = sortedDataset.getDistances( i );

		auto &idBinPair = binsById[ i ];
		idBinPair.first = idDistancePair.first;

		idBinPair.second.resize( numBins );

		for( auto distance = idDistancePair.second.begin() ; distance != idDistancePair.second.end() ; ++distance ) {
			if( *distance >= maxDistance ) {
				break;
			}

			const int binIndex = int(*distance / binWidth) * 2 + 1;
			idBinPair.second[ binIndex ]++;

			const int otherBinIndex = int((*distance + halfBinWidth) / binWidth) * 2;
			idBinPair.second[ otherBinIndex ]++;
		}
	}
}
