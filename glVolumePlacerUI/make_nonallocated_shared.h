#pragma once

#include <memory>

// wrap an object that has not been allocated in a shared object
template<typename T>
std::shared_ptr<T> make_nonallocated_shared(T &object) {
	// make this private for now
	struct null_deleter {		
		void operator() (T*) {}
	};

	return std::shared_ptr<T>( &object, null_deleter() );
}