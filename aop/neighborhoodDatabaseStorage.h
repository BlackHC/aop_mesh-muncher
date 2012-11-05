#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "neighborhoodDatabase.h"

SERIALIZER_DEFAULT_EXTERN_IMPL( Neighborhood::NeighborhoodContext,
	(distancesById)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( Neighborhood::SampledModel,
	(instances)
)