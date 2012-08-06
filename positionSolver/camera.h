#pragma once
#include <Eigen/Eigen>

#include "eigenProjectionMatrices.h"

struct Camera {
	const Eigen::Vector3f &getPosition() const { return position; }
	void setPosition(const Eigen::Vector3f &val) { position = val; }

	Eigen::Matrix3f getWorldToViewMatrix() const {
		return (Eigen::Matrix3f() << right, right.cross(forward), -forward).finished().transpose();
	}

	Eigen::Isometry3f getViewTransformation() const {
		Eigen::Isometry3f viewTransformation(getWorldToViewMatrix());
		viewTransformation.translate( -position );
		return viewTransformation;
	}

	struct PerspectiveProjectionParameters {
		float FoV_y;
		float aspect;
		float zNear, zFar;
	};
	PerspectiveProjectionParameters perspectiveProjectionParameters;

	Eigen::Matrix4f getProjectionMatrix() const {
		return Eigen::createPerspectiveProjectionMatrix( 
			perspectiveProjectionParameters.FoV_y,
			perspectiveProjectionParameters.aspect,
			perspectiveProjectionParameters.zNear,
			perspectiveProjectionParameters.zFar
			);
	}

	Camera() : position( Eigen::Vector3f::Zero() ), forward( -Eigen::Vector3f::UnitZ() ), right( Eigen::Vector3f::UnitX() ) {}

	void yaw( float degrees ) {
		auto rotation = Eigen::AngleAxisf( -degrees * Math::PI / 180.0, Eigen::Vector3f::UnitY() );
		forward = rotation._transformVector( forward );
		right = rotation._transformVector( right );

		forward.normalize();
		right.normalize();	
	}

	void pitch( float degrees ) {
		auto rotation = Eigen::AngleAxisf( -degrees * Math::PI / 180.0, right );
		forward = rotation._transformVector( forward );
		forward.normalize();
	}

private:
	Eigen::Vector3f position;

	// camera orientation
	Eigen::Vector3f forward, right;	
};