#include "validationStorage.h"

namespace Validation {
	const int CACHE_FORMAT_VERSION = 1;

	NeighborhoodData NeighborhoodData::load( const std::string &filename ) {
		Serializer::BinaryReader reader( filename.c_str(), CACHE_FORMAT_VERSION );
		if( reader.valid() ) {
			NeighborhoodData results;

			reader.get( results );

			return results;
		}

		logError( boost::format( "'%s' uses an old format!" ) % filename );
		return NeighborhoodData();
	}

	void NeighborhoodData::store( const std::string &filename, const NeighborhoodData &neighborhoodData ) {
		Serializer::BinaryWriter writer( filename.c_str(), CACHE_FORMAT_VERSION );

		writer.put( neighborhoodData );
	}
}