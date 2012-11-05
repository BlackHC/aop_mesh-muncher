#include "modelDatabaseStorage.h"

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