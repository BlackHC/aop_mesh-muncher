#pragma once

#include "neighborhoodDatabase.h"
#include "probeDatabase.h"

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

		void count( int modelIndex, int numInstances = 1 ) {
			instanceCounts[ modelIndex ] += numInstances;
			totalNumInstances +=numInstances; 
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

	struct ProbeSettings {
		float maxDistance;
		float positionVariance;
		int numSamples;
		float queryVolumeSize;
		float resolution;

		float distanceTolerance;
		float occlusionTolerance;
		float colorTolerance;

		ProbeSettings() {}
	};

	struct ProbeData {
		struct QueryData {
			ProbeContext::RawProbeSamples querySamples;
			ProbeContext::RawProbes queryProbes;
			Obb queryVolume;

			int expectedSceneModelIndex;

			QueryData() {}
			QueryData( QueryData &&other )
				: queryVolume( other.queryVolume )
				, expectedSceneModelIndex( other.expectedSceneModelIndex )
				, querySamples( std::move( other.querySamples ) )
				, queryProbes( std::move( other.queryProbes ) )
			{
			}

			QueryData & operator = ( QueryData &&other ) {
				querySamples = std::move( other.querySamples );
				queryProbes = std::move( other.queryProbes );
				queryVolume = other.queryVolume;

				expectedSceneModelIndex = other.expectedSceneModelIndex;

				return *this;
			}
		};

		std::vector<std::string> localModelNames;

		std::vector<QueryData> queries;

		InstanceCounts instanceCounts;

		ProbeSettings settings;

		ProbeData() {}
		ProbeData( int numModels, ProbeSettings settings )
			: instanceCounts( numModels )
			, settings( settings )
		{
		}

		static ProbeData load( const std::string &filename );
		static void store( const std::string &filename, const ProbeData &probeData );
	};
}