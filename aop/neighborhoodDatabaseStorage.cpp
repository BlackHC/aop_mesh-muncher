#include "neighborhoodDatabaseStorage.h"

namespace Neighborhood {
	const int CACHE_FORMAT_VERSION = 1;

	bool NeighborhoodDatabaseV2::load( const std::string &filename ) {
		Serializer::BinaryReader reader( filename.c_str(), CACHE_FORMAT_VERSION );
		if( reader.valid() ) {
			reader.get( numIds );
			reader.get( totalNumInstances );
			reader.get( sampledModelsById );


			return true;
		}
		logError( boost::format( "'%s' uses an old format!" ) % filename );
		return false;
	}

	void NeighborhoodDatabaseV2::store( const std::string &filename ) const {
		Serializer::BinaryWriter writer( filename.c_str(), CACHE_FORMAT_VERSION );
		writer.put( numIds );
		writer.put( totalNumInstances );
		writer.put( sampledModelsById );
	}

	RawIdDistances loadRawIdDistances( const std::string &filename ) {
		Serializer::BinaryReader reader( filename.c_str(), CACHE_FORMAT_VERSION );

		if( reader.valid() ) {
			RawIdDistances rawIdDistances;

			reader.get( rawIdDistances );

			return rawIdDistances;
		}

		logError( boost::format( "'%s' uses an old format!" ) % filename );
		return RawIdDistances();
	}

	void storeRawIdDistances( const std::string &filename, RawIdDistances &rawIdDistances ) {
		Serializer::BinaryWriter writer( filename.c_str(), CACHE_FORMAT_VERSION );
		writer.put( rawIdDistances );
	}

	Results loadResults( const std::string &filename ) {
		Serializer::BinaryReader reader( filename.c_str(), CACHE_FORMAT_VERSION );
		if( reader.valid() ) {
			Results results;

			reader.get( results );

			return results;
		}

		logError( boost::format( "'%s' uses an old format!" ) % filename );
		return Results();
	}

	void storeResults( const std::string &filename, const Results &results ) {
		Serializer::BinaryWriter writer( filename.c_str(), CACHE_FORMAT_VERSION );

		writer.put( results );
	}
}