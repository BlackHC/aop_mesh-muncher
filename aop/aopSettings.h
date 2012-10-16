#pragma once

#include <vector>
#include <Eigen/Eigen>
#include <mathUtility.h>
#include "camera.h"

namespace aop {
	struct Settings {
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
		std::vector<NamedModelGroup> modelGroups;

		float neighborhoodDatabase_queryTolerance;
		float neighborhoodDatabase_maxDistance;

		float probeGenerator_maxDistance;
		float probeGenerator_resolution;

		Settings() 
			: neighborhoodDatabase_queryTolerance( 1.0 )
			, neighborhoodDatabase_maxDistance( 150.0 )
			, probeGenerator_maxDistance( 5.0 )
			, probeGenerator_resolution( 0.25 ) 
		{}

		void load();
		void store() const;
	};
}