#pragma once

//#include <Eigen/Eigen>
#include <vector>
#include <map>

#include "sgsInterface.h"

struct ModelDatabase {
	struct ImportInterface {
		struct Tag {};
		virtual void sampleModel( int modelId, float resolution, Tag = Tag() ) = 0;
	};

	struct IdInformation {
		std::string name;
		std::string shortName;

		float volume;
		float area;
		float diagonalLength;

		float voxelResolution;

		typedef std::vector< SGSInterface::Probe > Probes;
		Probes probes;
		VoxelizedModel::Voxels voxels;

		IdInformation()
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
		IdInformation( IdInformation &&other )
			: name( std::move( other.name ) )
			, shortName( std::move( other.shortName ) )
			, volume( std::move( other.volume ) )
			, area( std::move( other.area ) )
			, diagonalLength( std::move( other.diagonalLength ) )
			, voxelResolution( std::move( other.voxelResolution ) )
			, probes( std::move( other.probes ) )
			, voxels( std::move( other.voxels ) )
		{}

		IdInformation & operator = ( IdInformation &&other ) {
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

	std::vector< IdInformation > informationById;

	ModelDatabase( ImportInterface *importInterface ) : importInterface( importInterface ) {}

	const IdInformation::Probes & getProbes( int modelId, float resolution ) {
		auto &model = informationById[ modelId ];

		if( model.voxelResolution != resolution ) {
			importInterface->sampleModel( modelId, resolution );
		}

		return model.probes;
	}

	// see candidateFinderCache.cpp
	bool load( const char *filename );
	void store( const char *filename );
};