#pragma once

// TODO: add a serializer forward header with macros to declare these functions [10/3/2012 kirschan2]
struct SortedProbeDataset;
struct IndexedProbeDataset;

#define SERIALIZER_FWD_EXTERN_DECL( type ) \
	namespace Serializer { \
		template< typename Reader > \
		void read( Reader &reader, type &value ); \
		template< typename Writer > \
		void write( Writer &writer, const type &value ); \
	}

#define SERIALIZER_FWD_FRIEND_EXTERN( type ) \
	template< typename Reader > \
	friend void Serializer::read( Reader &reader, type &value ); \
	template< typename Writer > \
	friend void Serializer::write( Writer &writer, const type &value );
