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

SERIALIZER_DEFAULT_EXTERN_IMPL( SortedProbeDataset, (data) )
SERIALIZER_DEFAULT_EXTERN_IMPL( IndexedProbeDataset, (data)(hitCounterLowerBounds) )
SERIALIZER_DEFAULT_EXTERN_IMPL( IdDatasets, (instances)(mergedInstances)(probes) )

SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::ProbeContext );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::Probe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( SortedProbeDataset::ProbeContext );

const int CACHE_FORMAT_VERSION = 2;

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