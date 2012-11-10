#pragma once

#include <vector>
#include <Eigen/Eigen>
#include <mathUtility.h>
#include "camera.h"

#include "probeDatabase.h"

namespace aop {
	//const char const *
	struct Settings {
		std::string scenePath;
		std::string sceneSettingsPath;
		std::string probeDatabasePath;
		std::string modelDatabasePath;
		std::string neighborhoodDatabaseV2Path;

		std::string neighborhoodValidationDataPath;
		std::string probeValidationDataPath;

		float validation_neighborhood_positionVariance;
		int validation_neighborhood_numSamples;

		float validation_probes_positionVariance;
		int validation_probes_numSamples;
		float validation_probes_queryVolumeSize;

		Settings();

		void load();
		void store() const;
	};

	struct SceneSettings {
		struct CameraState {
			Eigen::Vector3f position;
			Eigen::Vector3f direction;

			void pushTo( Camera &camera ) {
				camera.setPosition( position );
				camera.lookAt( direction, Eigen::Vector3f::UnitY() );
			}

			void pullFrom( const Camera &camera ) {
				position = camera.getPosition();
				direction = camera.getDirection();
			}
		};

		struct NamedCameraState : CameraState {
			std::string name;
		};

		struct NamedTargetVolume {
			std::string name;
			Obb volume;
		};

		typedef std::vector< std::string > ModelGroup;

		struct NamedModelGroup {
			std::string name;
			ModelGroup models;
		};

		std::vector<NamedCameraState> views;
		std::vector<NamedTargetVolume> volumes;
		ModelGroup markedModels;

		float neighborhoodDatabase_queryTolerance;
		float neighborhoodDatabase_maxDistance;

		float probeGenerator_maxDistance;
		float probeGenerator_resolution;

		float probeQuery_occlusionTolerance;
		float probeQuery_distanceTolerance;
		float probeQuery_colorTolerance;

		SceneSettings()
			: neighborhoodDatabase_queryTolerance( 1.0 )
			, neighborhoodDatabase_maxDistance( 150.0 )
			, probeGenerator_maxDistance( 5.0 )
			, probeGenerator_resolution( 0.25 )
			, probeQuery_occlusionTolerance( 0.125 )
			, probeQuery_distanceTolerance( 0.25 )
			, probeQuery_colorTolerance( 1.0 )
		{}

		void load( const std::string &filename );
		void store( const std::string &filename ) const;
	};
}