#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "modelDatabase.h"

/*
namespace Serializer {
		template< typename Reader >
		void read( Reader &reader, type &value ) {
			
		}
		template< typename Writer > \
		void write( Writer &writer, const type &value ) {
			
		}
	}*/

SERIALIZER_DEFAULT_EXTERN_IMPL( IndexMapping3<>, (size)(count)(indexToPosition)(positionToIndex) )
SERIALIZER_DEFAULT_EXTERN_IMPL( VoxelizedModel::Voxels, (mapping)(data) )
SERIALIZER_DEFAULT_EXTERN_IMPL( ModelDatabase::IdInformation, (name)(shortName)(volume)(area)(diagonalLength)(probes)(voxels) )

SERIALIZER_ENABLE_RAW_MODE_EXTERN( SGSInterface::Probe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( VoxelizedModel::NormalOverdraw4ub );

const int CACHE_FORMAT_VERSION = 0;

bool ModelDatabase::load( const char *filename ) {
	Serializer::BinaryReader reader( filename, CACHE_FORMAT_VERSION );
	if( reader.valid() ) {
		reader.get( informationById );
		return true;
	}
	return false;
}

void ModelDatabase::store( const char *filename ) {
	Serializer::BinaryWriter writer( filename, CACHE_FORMAT_VERSION );
	writer.put( informationById );
}