#include "validationStorage.h"

namespace Validation {
	const int CACHE_FORMAT_VERSION = 1;

	bool NeighborhoodData::load( const std::string &filename, NeighborhoodData &neighborhoodData ) {
		Serializer::BinaryReader reader( filename, CACHE_FORMAT_VERSION );
		if( reader.valid() ) {
			reader.get( neighborhoodData );

			return true;
		}

		logError( boost::format( "'%s' uses an old format or is missing!" ) % filename );
		return false;
	}

	void NeighborhoodData::store( const std::string &filename, const NeighborhoodData &neighborhoodData ) {
		Serializer::BinaryWriter writer( filename, CACHE_FORMAT_VERSION );

		writer.put( neighborhoodData );
	}

	bool ProbeData::load( const std::string &filename, ProbeData &probeData ) {
		Serializer::BinaryReader reader( filename, CACHE_FORMAT_VERSION );
		if( reader.valid() ) {
			reader.get( probeData );

			return true;
		}

		logError( boost::format( "'%s' uses an old format!" ) % filename );
		return false;
	}

	void ProbeData::store( const std::string &filename, const ProbeData &probeData ) {
		Serializer::BinaryWriter writer( filename, CACHE_FORMAT_VERSION );

		writer.put( probeData );
	}
}