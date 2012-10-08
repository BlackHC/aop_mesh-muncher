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

		std::vector<NamedCameraState> views;
		std::vector<NamedTargetVolume> volumes;

		void load();
		void store() const;
	};
}