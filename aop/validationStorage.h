#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "validation.h"

SERIALIZER_DEFAULT_EXTERN_IMPL( Validation::InstanceCounts,
	(instanceCounts)
	(totalNumInstances)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( Validation::NeighborhoodData,
	(maxDistance)
	(queryDatasets)
	(queryInfos)
	(instanceCounts)
)