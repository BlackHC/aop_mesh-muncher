#define SERIALIZER_SUPPORT_STL
#include <serializer.h>

#include "probeDatabase.h"

/*
namespace Serializer {
		template< typename Reader >
		void read( Reader &reader, type &value ) {
			
		}
		template< typename Writer > \
		void write( Writer &writer, const type &value ) {
			
		}
	}*/

SERIALIZER_DEFAULT_EXTERN_IMPL( RawProbeDataset, (probes)(probeContexts) )
SERIALIZER_DEFAULT_EXTERN_IMPL( SortedProbeDataset, (data) )
SERIALIZER_DEFAULT_EXTERN_IMPL( IndexedProbeDataset, (data)(hitCounterLowerBounds) )
SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeDatabase::IdDatasets, (insertQueue)(mergedDataset) )

SERIALIZER_ENABLE_RAW_MODE_EXTERN( RawProbeDataset::Probe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( RawProbeDataset::ProbeContext );

const int CACHE_FORMAT_VERSION = 0;

bool ProbeDatabase::loadCache( const char *filename ) {
	Serializer::BinaryReader reader( filename, CACHE_FORMAT_VERSION );
	if( reader.valid() ) {
		reader.get( idDatasets );
		return true;
	}
	return false;
}

void ProbeDatabase::storeCache( const char *filename ) {
	Serializer::BinaryWriter writer( filename, CACHE_FORMAT_VERSION );
	writer.put( idDatasets );
}