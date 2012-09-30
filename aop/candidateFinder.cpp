#include "candidateFinderInterface.h"

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