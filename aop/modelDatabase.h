#pragma once

//#include <Eigen/Eigen>
#include <vector>
#include <map>

#include "sgsInterface.h"
#include "probeGenerator.h"

struct ModelDatabase {
	typedef int ModelIndex;
	typedef std::vector< std::string > ModelGroup;
	typedef std::vector< int > ModelIndexGroup;

	static const ModelIndex INVALID_MODEL_INDEX = -1;

	struct ImportInterface {
		struct Tag {};
		virtual void sampleModel( int modelId, float resolution, Tag = Tag() ) = 0;
	};

	struct ModelInformation {
		std::string name;
		std::string shortName;

		float volume;
		float area;
		float diagonalLength;

		float voxelResolution;

		typedef ProbeGenerator::Probes Probes;
		Probes probes;
		VoxelizedModel::Voxels voxels;

		ModelInformation()
			: name()
			, shortName()
			, volume()
			, area()
			, diagonalLength()
			, voxelResolution()
			, probes()
			, voxels()
		{}

		// TODO: add move semantics [10/13/2012 kirschan2]
		ModelInformation( ModelInformation &&other )
			: name( std::move( other.name ) )
			, shortName( std::move( other.shortName ) )
			, volume( std::move( other.volume ) )
			, area( std::move( other.area ) )
			, diagonalLength( std::move( other.diagonalLength ) )
			, voxelResolution( std::move( other.voxelResolution ) )
			, probes( std::move( other.probes ) )
			, voxels( std::move( other.voxels ) )
		{}

		ModelInformation & operator = ( ModelInformation &&other ) {
			name = std::move( other.name );
			shortName = std::move( other.shortName );
			volume = std::move( other.volume );
			area = std::move( other.area );
			diagonalLength = std::move( other.diagonalLength );
			voxelResolution = std::move( other.voxelResolution );
			probes = std::move( other.probes );
			voxels = std::move( other.voxels );

			return *this;
		}
	};

	ImportInterface *importInterface;

	std::vector< ModelInformation > informationById;

	ModelDatabase( ImportInterface *importInterface ) : importInterface( importInterface ) {}

	const ModelInformation::Probes & getProbes( int modelId, float resolution ) {
		auto &model = informationById[ modelId ];

		if( model.voxelResolution != resolution ) {
			importInterface->sampleModel( modelId, resolution );
		}

		return model.probes;
	}

	// see candidateFinderCache.cpp
	bool load( const char *filename );
	void store( const char *filename );

#if 0
	void registerModel( int modelIndex, IdInformation &&informationById );
#endif

	//////////////////////////////////////////////////////////////////////////
	// TODO: move this into an idConverter class? [10/15/2012 kirschan2]
	std::map< std::string, ModelIndex > modelNameIdMap;
		
	ModelIndex convertToModelIndex( const std::string &name ) {
		auto it = modelNameIdMap.find( name );
		if( it != modelNameIdMap.end() ) {
			return it->second;
		}
		return INVALID_MODEL_INDEX;
	}

	ModelIndexGroup convertToModelIndices( const ModelGroup &group ) {
		ModelIndexGroup converted;
		converted.reserve( group.size() );
		
		for( auto modelName = group.begin() ; modelName != group.end() ; ++modelName ) {
			ModelIndex modelIndex = convertToModelIndex( *modelName );
			if( modelIndex != INVALID_MODEL_INDEX ) {
				converted.push_back( modelIndex );
			}
		}

		return converted;
	}

	std::string convertToModelName( ModelIndex modelIndex ) {
		if( modelIndex >= informationById.size() ) {
			return std::string();
		}

		return informationById[ modelIndex ].name;
	}

	ModelGroup convertToModelGroup( const ModelIndexGroup &indexGroup ) {
		ModelGroup converted;
		converted.reserve( indexGroup.size() );

		for( auto modelIndex = indexGroup.begin() ; modelIndex != indexGroup.end() ; ++modelIndex ) {
			std::string modelName = informationById[ *modelIndex ].name;

			if( !modelName.empty() ) {
				converted.emplace_back( std::move( modelName ) );
			}
		}

		return converted;
	}
};