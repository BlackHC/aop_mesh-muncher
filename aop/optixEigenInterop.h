#pragma once

#include <Eigen/Eigen>
#include <optix_world.h>

// optix eigen wrapper
namespace Eigen {
	Eigen::Vector3f::MapType map( optix::float3 &v ) {
		return Eigen::Vector3f::Map( &v.x );
	}

	Eigen::Vector3f::ConstMapType map( const optix::float3 &v ) {
		return Eigen::Vector3f::Map( &v.x );
	}
}