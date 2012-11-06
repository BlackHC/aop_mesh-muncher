#pragma once

#include "neighborhoodDatabase.h"

namespace Validation {
	/*struct InstanceInfo {
		int instanceIndex;
		int modelIndex;
		Eigen::Affine3f transformation;
	};*/

	typedef std::vector< Neighborhood::RawIdDistances > NeighborhoodQueryDatasets;

	struct InstanceCounts {
		std::vector<int> instanceCounts;
		int totalNumInstances;

		InstanceCounts() {}
		InstanceCounts( int numModels ) : instanceCounts( numModels ), totalNumInstances() {}

		void count( int modelIndex ) {
			instanceCounts[ modelIndex ] += 1;
			totalNumInstances +=1; 
		}

		float calculateRankExpectation() const;
	};

	struct NeighborhoodSettings {
		float maxDistance;
		float positionVariance;
		int numSamples;

		NeighborhoodSettings() {}

		NeighborhoodSettings( int numSamples, float maxDistance, float positionVariance )
			: numSamples( numSamples )
			, maxDistance( maxDistance )
			, positionVariance( positionVariance )
		{}
	};

	struct NeighborhoodData {
		NeighborhoodSettings settings;

		NeighborhoodQueryDatasets queryDatasets;
		std::vector<int> queryInfos;

		InstanceCounts instanceCounts;

		NeighborhoodData() {}
		NeighborhoodData( int numModels, NeighborhoodSettings settings )
			: instanceCounts( numModels )
			, settings( settings )
		{}

		static NeighborhoodData load( const std::string &filename );
		static void store( const std::string &filename, const NeighborhoodData &neighborhoodData );
	};
}