#define SERIALIZER_SUPPORT_STL
#include <serializer.h>

#include "candidateFinderInterface.h"

SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeDataset, (probes)(probeContexts)(hitCounterLowerBounds) )
SERIALIZER_DEFAULT_EXTERN_IMPL( CandidateFinder::IdDatasets, (insertQueue)(mergedDataset) )

SERIALIZER_ENABLE_RAW_MODE_EXTERN( Probe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( ProbeContext );

const int CACHE_FORMAT_VERSION = 0;

bool CandidateFinder::loadCache( const char *filename ) {
	Serializer::BinaryReader reader( filename, CACHE_FORMAT_VERSION );
	if( reader.valid() ) {
		reader.get( idDatasets );
		return true;
	}
	return false;
}

void CandidateFinder::storeCache( const char *filename ) {
	Serializer::BinaryWriter writer( filename, CACHE_FORMAT_VERSION );
	writer.put( idDatasets );
}