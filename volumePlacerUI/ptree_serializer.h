#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/lexical_cast.hpp>

/*
serialize pointers using helper objects and an object id/type map.. 
OR:

using an interval set
serialize_allocated_object( T* ) adds a void *pointer and sizeof( T ) to an interval set

serialize_pointer searches for the pointer in the interval set and stores an id and relative offset
serialize_external_pointer replaces a pointer with a fixed id that has been set before (otherwise it wont work)

this will work for binary storage but makes it difficult for a human to change anything...
*/
/*
struct ptree_serializer {
	template<typename T>
	void exchange( const char *key, T &data, const T &defaultValue = T());
};
*/

// FIXME: const-correctness does not work with exchange :(
// solution: use specialized classes instead of specialized functions.. :)

template<typename T>
void ptree_serializer_read( boost::property_tree::ptree &tree, T &data ) {
	data = tree.get_value<T>();
}

template<typename T>
void ptree_serializer_write( boost::property_tree::ptree &tree, const T &data ) {
	tree.put_value( data );
}

enum ptree_serializer_mode {
	PSM_READING,
	PSM_WRITING
};

template<ptree_serializer_mode mode, typename T>
void ptree_serializer_exchange( boost::property_tree::ptree &tree, T &data ) {
	switch( mode ) {
	case PSM_READING:
		ptree_serializer_read( tree, data );
		break;
	case PSM_WRITING:
		ptree_serializer_write( tree, data );
		break;
	}
}

template<typename T>
void ptree_serializer_get( boost::property_tree::ptree &tree, const char *key, T &data, const T& defaultValue ) {
	auto it = tree.find( key );
	if( it != tree.not_found() ) {
		ptree_serializer_exchange<PSM_READING>( *it, data );
	}
	else {
		data = defaultValue;
	}
}

template<typename T>
void ptree_serializer_get( boost::property_tree::ptree &tree, const char *key, T &data ) {
	ptree_serializer_exchange<PSM_READING>( tree.get_child( key ), data );
}

template<typename T>
void ptree_serializer_put( boost::property_tree::ptree &tree, const char *key, T &data ) {
	boost::property_tree::ptree &subTree = tree.add_child( key, boost::property_tree::ptree() );
	ptree_serializer_exchange<PSM_WRITING>( subTree, data );
}

template<ptree_serializer_mode mode, typename T>
void ptree_serialize( boost::property_tree::ptree &tree, const char *key, T &data ) {
	switch( mode ) {
	case PSM_READING:
		ptree_serializer_get( tree, key, data );
		break;
	case PSM_WRITING:
		ptree_serializer_put( tree, key, data );
		break;
	}
}

// special functions
template<typename T>
void ptree_serializer_read( boost::property_tree::ptree &tree, std::vector<T> &data ) {
	data.reserve( tree.size() );

	for( auto it = tree.begin() ; it != tree.end() ; ++it ) {
		T value;
		ptree_serializer_exchange<PSM_READING>( it->second, value );
		data.push_back( std::move( value ) );			
	}
}

template<typename T>
void ptree_serializer_write( boost::property_tree::ptree &tree, std::vector<T> &data ) {
	for( auto it = data.begin() ; it != data.end() ; ++it ) {
		ptree_serialize<PSM_WRITING>( tree, "item", *it );
	}
}

// static dispatch
/*
template< typename Target >
struct ptree_static_dispatch {
	struct StaticTag {};

	template< typename Vistor, typename Param, typename Result >
	static void static_visit( const Param &p ) {
		Vistor::static_visit( p, StaticTag );
	}
};

struct ptree_static_construction {

};*/