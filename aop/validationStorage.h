#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "validation.h"

#include "probeDatabaseStorage.h"

SERIALIZER_DEFAULT_EXTERN_IMPL( Validation::InstanceCounts,
	(instanceCounts)
	(totalNumInstances)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( Validation::NeighborhoodSettings,
	(numSamples)
	(maxDistance)
	(positionVariance)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( Validation::NeighborhoodData,
	(settings)
	(queryDatasets)
	(queryInfos)
	(instanceCounts)
)

/*SERIALIZER_DEFAULT_EXTERN_IMPL( Validation::ProbeSettings,
	(maxDistance)
	(resolution)

	(queryVolumeSize)

	(numSamples)
	(positionVariance)
)*/

SERIALIZER_ENABLE_RAW_MODE_EXTERN( Validation::ProbeSettings );

SERIALIZER_DEFAULT_EXTERN_IMPL( Validation::ProbeData::QueryData,
	(querySamples)
	(queryProbes)
	(queryVolume)
	(expectedSceneModelIndex)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( Validation::ProbeData,
	(settings)
	(queries)
	(instanceCounts)
	(localModelNames)
)

