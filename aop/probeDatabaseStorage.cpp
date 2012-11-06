#include "probeDatabaseStorage.h"

const int CACHE_FORMAT_VERSION = 4;

namespace ProbeContext {
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
}