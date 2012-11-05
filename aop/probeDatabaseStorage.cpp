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

BOOST_STATIC_ASSERT( sizeof( RawProbe ) == 4 );
BOOST_STATIC_ASSERT( sizeof( RawProbeSample ) == 8 );
BOOST_STATIC_ASSERT( sizeof( DBProbeSample ) == 8 + 8 );

SERIALIZER_DEFAULT_EXTERN_IMPL( SampledModel::SampledInstance, (source)(probeSamples) )
SERIALIZER_DEFAULT_EXTERN_IMPL( IndexedProbeSamples, (data)(occlusionLowerBounds) )
SERIALIZER_DEFAULT_EXTERN_IMPL( SampledModel,
	(instances)
	(mergedInstances)
	(mergedInstancesByDirectionIndex)
	(probes)
	(rotatedProbePositions)
	(resolution)
)

SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::ProbeSample );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( RawProbe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( DBProbeSample );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( QueryResult );

// TODO: this is a duplicate from aopSettingsStorage.cpp---add a storage header instead? [10/22/2012 kirschan2]
SERIALIZER_DEFAULT_EXTERN_IMPL( Obb, (transformation)(size) );

const int CACHE_FORMAT_VERSION = 4;

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

namespace IO {
	void loadRawQuery( const std::string &filename, RawProbes &probes, RawProbeSamples &probeSamples ) {
		Serializer::BinaryReader reader( filename.c_str(), CACHE_FORMAT_VERSION );
		if( reader.valid() ) {
			reader.get( probes );
			reader.get( probeSamples );
		}
		logError( boost::format( "'%s' uses an old format!" ) % filename );
	}

	void storeRawQuery( const std::string &filename, const RawProbes &probes, const RawProbeSamples &probeSamples ) {
		Serializer::BinaryWriter writer( filename.c_str(), CACHE_FORMAT_VERSION );
		writer.put( probes );
		writer.put( probeSamples );
	}

	QueryResults loadQueryResults( const std::string &filename ) {
		Serializer::BinaryReader reader( filename.c_str(), CACHE_FORMAT_VERSION );
		if( reader.valid() ) {
			QueryResults queryResults;

			reader.get( queryResults );

			return queryResults;
		}
		logError( boost::format( "'%s' uses an old format!" ) % filename );
		return QueryResults();
	}

	void storeQueryResults( const std::string &filename, const QueryResults &results ) {
		Serializer::BinaryWriter writer( filename.c_str(), CACHE_FORMAT_VERSION );
		writer.put( results );
	}
}

