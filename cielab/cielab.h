#pragma once

#include <Eigen/Eigen>

namespace ColorConversion {
	Eigen::Vector3f RGB_to_CIELab( const Eigen::Vector3f &rgb );
	Eigen::Vector3f CIELab_to_RGB( const Eigen::Vector3f &lab );
}