#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
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

SERIALIZER_DEFAULT_EXTERN_IMPL( SampledModel::SampledInstance, (source)(probeContexts) )
SERIALIZER_DEFAULT_EXTERN_IMPL( IndexedProbeContexts, (data)(hitCounterLowerBounds) )
SERIALIZER_DEFAULT_EXTERN_IMPL( SampledModel, (instances)(mergedInstances)(probes) )

SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::ProbeContext );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::Probe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( DBProbeContext );

// TODO: this is a duplicate from aopSettingsStorage.cpp---add a storage header instead? [10/22/2012 kirschan2]
SERIALIZER_DEFAULT_EXTERN_IMPL( Obb, (transformation)(size) );

const int CACHE_FORMAT_VERSION = 3;

bool ProbeDatabase::load( const std::string &filename ) {
	Serializer::BinaryReader reader( filename.c_str(), CACHE_FORMAT_VERSION );
	if( reader.valid() ) {
		reader.get( localModelNames );
		reader.get( sampledModels );
	
		modelIndexMapper.registerLocalModels( localModelNames );

		return true;
	}
	return false;
}

void ProbeDatabase::store( const std::string &filename ) const {
	Serializer::BinaryWriter writer( filename.c_str(), CACHE_FORMAT_VERSION );
	writer.put( localModelNames );
	writer.put( sampledModels );
}
