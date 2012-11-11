#pragma once

#include <vector>
#include <functional>
#include <algorithm>


// everything is stored in a flat vector
// sorted by Key and T
template<
	typename Key,
	typename T,
	typename BucketIndex = size_t,
	typename ItemIndex = size_t,
	class KeyHash = std::hash<Key>
	//class TPred = std::less<T>
>
struct FlatImmutableOrderedMultiMap {
	typedef Key key_type;
	typedef T value_type;
	typedef KeyHash key_hash_type;
	//typedef TPred item_predicate;
	typedef ItemIndex item_index_type;
	typedef BucketIndex bucket_index_type;

	typedef std::pair<key_type, value_type> item_type;
	typedef std::vector<item_type> items_type;

	typedef std::vector<item_index_type> buckets_type;

	typedef typename items_type::const_iterator const_iterator;
	typedef std::pair< const_iterator, const_iterator > const_iterator_range;

	items_type items;
	buckets_type bucketOffsets;

	key_hash_type key_hash;

	FlatImmutableOrderedMultiMap( int numBuckets = 0, key_hash_type &&key_hash = key_hash_type() )
		: key_hash( std::move( key_hash ) )
	{
		init( numBuckets );
	}

	void init( int numBuckets ) {
		items.clear();

		bucketOffsets.clear();
		bucketOffsets.resize( numBuckets + 1 );
	}

	bool empty() const {
		return items.empty();
	}

	bucket_index_type getNumBuckets() const {
		return (bucket_index_type) bucketOffsets.size() - 1;
	}

	item_index_type getNumElements() const {
		return (item_index_type) items.size();
	}

	bucket_index_type getBucketIndex( const key_type &key ) const {
		return key_hash( key ) % getNumBuckets();
	}

	const_iterator_range equal_range( const key_type &key ) const {
		struct KeyItemLess {
			bool operator () ( const key_type &key, const item_type &item ) {
				return key < item.first;
			}

			bool operator () ( const item_type &item, const key_type &key ) {
				return item.first < key;
			}

			bool operator () ( const item_type &itemA, const item_type &itemB ) {
				return itemA.first < itemB.first;
			}
		};

		// determine the bucket
		const auto bucketIndex = getBucketIndex( key );
		// search inside the bucket
		const auto bucketBegin = items.begin() + bucketOffsets[ bucketIndex ];
		const auto bucketEnd = items.begin() + bucketOffsets[ bucketIndex + 1 ];
		return std::equal_range( bucketBegin, bucketEnd, key, KeyItemLess() );
	}


	struct Builder {
		FlatImmutableOrderedMultiMap &multiMap;

		std::vector<item_index_type> bucketCounters;
		std::vector< std::pair< bucket_index_type, item_type > > unsortedItems;

		Builder( FlatImmutableOrderedMultiMap &multiMap, int numElements = 0 )
			: multiMap( multiMap )
		{
			bucketCounters.clear();
			bucketCounters.resize( multiMap.getNumBuckets() );

			if( numElements > 0 ) {
				multiMap.items.reserve( numElements );
				unsortedItems.reserve( numElements );
			}
		}

		void push_back( const key_type &key, const value_type &value ) {
			const auto bucketIndex = multiMap.getBucketIndex( key );
			bucketCounters[ bucketIndex ]++;
			unsortedItems.push_back( std::make_pair( bucketIndex, std::make_pair( key, value ) ) );
		}

		void build() {
			// set the bucket offsets
			multiMap.bucketOffsets[ 0 ] = 0;
			for( bucket_index_type bucketIndex = 0 ; bucketIndex < multiMap.getNumBuckets() ; bucketIndex++ ) {
				multiMap.bucketOffsets[ bucketIndex + 1 ] = multiMap.bucketOffsets[ bucketIndex ] + bucketCounters[ bucketIndex ];
			}

			// allocate the data
			multiMap.items.resize( unsortedItems.size() );

			// now count the the objects into their buckets
			for( auto unsortedItem = unsortedItems.begin() ; unsortedItem != unsortedItems.end() ; unsortedItem++ ) {
				const auto bucketIndex = unsortedItem->first;
				// example: bucketOffsets 0,3... we have 3 items for bucket 0, bucketCounter[0] = 3
				// we insert the first object at 3-3 and then decrement bucketCounter
				const auto itemIndex = multiMap.bucketOffsets[ bucketIndex + 1 ] - bucketCounters[ bucketIndex ]--;
				multiMap.items[ itemIndex ] = std::move( unsortedItem->second );

				if( bucketCounters[ bucketIndex ] == 0 ) {
					// this bucket is done
					// we now sort the range
					std::sort( multiMap.items.begin() + multiMap.bucketOffsets[ bucketIndex ], multiMap.items.begin() + multiMap.bucketOffsets[ bucketIndex + 1 ] );
				}
			}

			// all done, clear up
			bucketCounters.clear();
			unsortedItems.clear();
		}
	};

	Builder createBuilder( int expectedNumElements = 0 ) {
		return Builder( *this, expectedNumElements );
	}
};