#pragma once
#include <Eigen/Eigen>

#include "eigenProjectionMatrices.h"

struct Camera {
	const Eigen::Vector3f &getPosition() const { return position; }

	// world to view rotation
	Eigen::Matrix3f getViewRotation() const {
		return (Eigen::Matrix3f() << right, right.cross(forward), -forward).finished().transpose();
	}

	Eigen::Isometry3f getViewTransformation() const {
		Eigen::Isometry3f viewTransformation(getViewRotation());
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
		auto rotation = Eigen::AngleAxisf( -degrees * float(Math::PI) / 180.0f, Eigen::Vector3f::UnitY() );
		forward = rotation._transformVector( forward );
		right = rotation._transformVector( right );

		forward.normalize();
		right.normalize();	
	}

	void pitch( float degrees ) {
		auto rotation = Eigen::AngleAxisf( -degrees * float(Math::PI) / 180.0f, right );
		forward = rotation._transformVector( forward );
		forward.normalize();
	}

	void setPosition( const Eigen::Vector3f &newPosition ) {
		position = newPosition;
	}

	void lookAt( const Eigen::Vector3f &newForward, const Eigen::Vector3f &up ) {
		forward = newForward.normalized();
		right = forward.cross( up ).normalized();
	}

	const Eigen::Vector3f &getDirection() const {
		return forward;
	}

private:
	Eigen::Vector3f position;

	// camera orientation
	Eigen::Vector3f forward, right;	
};