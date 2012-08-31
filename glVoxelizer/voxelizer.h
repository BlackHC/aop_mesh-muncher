#pragma once

#include "gridStorage.h"

#include <functional>

struct Color4ub {
	unsigned char r,g,b,a;
};

typedef GridStorage<Color4ub> ColorGrid;

void voxelizeScene( const SimpleIndexMapping3 &indexMapping3, ColorGrid &grid, std::function<void()> renderScene );