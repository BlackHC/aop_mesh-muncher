#pragma once

#include "niven.Volume.BlockStorage.h"
#include <map>

namespace niven {
	namespace Volume {
		class MemoryBlockStorage : public IBlockStorage {
		private:
			virtual bool AddBlocksImpl( const String& layerName, const ArrayRef<Id>& ids, const ArrayRef<ArrayRef<>>& buffers ) 
			{
				auto &layer = layers[ layerName ];
				for( int i = 0 ; i < ids.GetCount() ; i++ ) {
					layer[ ids[i] ] = std::vector<byte>( (byte*) buffers[i].GetData(), (byte*) buffers[i].GetData() + buffers[i].GetSize() );
				}
				return true;
			}

			virtual bool GetBlocksImpl( const String& layerName, const ArrayRef<Id>& ids, const ArrayRef<MutableArrayRef<>>& buffers ) 
			{
				auto &layer = layers[ layerName ];
				for( int i = 0 ; i < ids.GetCount() ; i++ ) {
					const auto &block = layer[ ids[i] ];
					std::copy( block.begin(), block.end(), (byte*) buffers[i].GetData() );
				}
				return true;
			}

			virtual bool RemoveBlocksImpl( const String& layerName, const ArrayRef<Id>& ids ) 
			{
				auto &layer = layers[ layerName ];
				for( int i = 0 ; i < ids.GetCount() ; i++ ) {
					layer.erase( ids[i] );
				}
				return true;
			}

			virtual bool ContainsBlocksImpl( const String& layerName, const ArrayRef<Id>& ids, const MutableArrayRef<bool>& resultBuffer ) const
			{
				auto it = layers.find( layerName );
				if( it == layers.end() ) {
					std::fill( resultBuffer.begin(), resultBuffer.end(), false );
					return true;				
				}
				auto &layer = it->second;
				for( int i = 0 ; i < ids.GetCount() ; i++ ) {
					resultBuffer[i] = layer.find( ids[i] ) != layer.end();
				}
				return true;
			}

			virtual bool GetBlockDataSizesImpl( const String& layerName, const ArrayRef<Id>& ids, const MutableArrayRef<int>& sizeBuffer ) const
			{
				auto it = layers.find( layerName );
				if( it == layers.end() ) {
					std::fill( sizeBuffer.begin(), sizeBuffer.end(), 0 );
					return true;
				}
				auto &layer = it->second;
				for( int i = 0 ; i < ids.GetCount() ; i++ ) {
					auto it = layer.find( ids[i] );
					if( it != layer.end() ) {
						sizeBuffer[i] = it->second.size();
					}
					else {
						sizeBuffer[i] = 0;
					}
				}
				return true;
			}

			virtual std::vector<Id> GetBlockIdsImpl( const String& layerName ) const
			{
				auto it = layers.find( layerName );
				if( it == layers.end() ) {
					return std::vector<Id>();
				}
				auto &layer = it->second;
				std::vector<Id> blockIds;
				for( auto it = layer.begin() ; it != layer.end() ; it++ ) {
					blockIds.push_back( it->first );
				}
				return blockIds;
			}

			virtual void AddLayerImpl( const String& name ) 
			{
				layerNames.push_back( name );
			}

			virtual void RemoveLayerImpl( const String& name ) 
			{
				auto it = std::find( layerNames.begin(), layerNames.end(), name );
				if( it != layerNames.end() ) {
					layerNames.erase( it );
					layers.erase( name );
				}
			}

			virtual std::vector<String> GetLayerNamesImpl() const
			{
				return layerNames;
			}

			virtual void SetAttributeImpl( const String& key, const ArrayRef<>& value ) 
			{
				attributes[ key ].assign( (byte*) value.GetData(), (byte*) value.GetData() + value.GetSize() );
			}

			virtual bool HasAttributeImpl( const String& key ) const
			{
				return attributes.find( key ) != attributes.end();
			}

			virtual void GetAttributeImpl( const String& key, const MutableArrayRef<>& value ) const
			{
				auto it = attributes.find( key );
				if( it == attributes.end() ) {
					return;
				}
				auto &attribute = it->second;
				std::copy( attribute.begin(), attribute.end(), (byte*) value.GetData() );
			}

			virtual int GetAttributeSizeImpl( const String& key ) const
			{
				return attributes.find( key )->second.size();
			}

			virtual void RemoveAttributeImpl( const String& key ) 
			{
				attributes.erase( key );
			}

		private:
			struct CompareIds {
				bool operator() ( const Id &a, const Id &b ) const {
					return VectorLexicographicCompare<>()( a.position, b.position) || (a.position == b.position && a.mip < b.mip);
				}
			};

			// TODO: use unordered_map and boost::hash
			std::vector< String > layerNames;
			std::map< String, std::map< Id, std::vector<byte>, CompareIds> > layers;
			std::map< String, std::vector<byte> > attributes;
		};
	}
}
