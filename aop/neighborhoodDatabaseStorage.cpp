
#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "neighborhoodDatabase.h"

SERIALIZER_DEFAULT_EXTERN_IMPL( Neighborhood::NeighborhoodContext,
	(distancesById)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( Neighborhood::SampledModel,
	(instances)
)

namespace Neighborhood {
	const int CACHE_FORMAT_VERSION = 0;

	bool NeighborhoodDatabaseV2::load( const std::string &filename ) {
		Serializer::BinaryReader reader( filename.c_str(), CACHE_FORMAT_VERSION );
		if( reader.valid() ) {
			reader.get( numIds );
			reader.get( sampledModelsById );

			return true;
		}
		return false;
	}

	void NeighborhoodDatabaseV2::store( const std::string &filename ) const {
		Serializer::BinaryWriter writer( filename.c_str(), CACHE_FORMAT_VERSION );
		writer.put( numIds );
		writer.put( sampledModelsById );
	}
}