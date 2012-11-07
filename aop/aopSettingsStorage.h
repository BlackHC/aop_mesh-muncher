#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "aopSettings.h"

SERIALIZER_DEFAULT_EXTERN_IMPL( Obb, (transformation)(size) );

SERIALIZER_EXTERN_IMPL( aop::SceneSettings::NamedCameraState, name, (position)(direction), );
SERIALIZER_EXTERN_IMPL( aop::SceneSettings::NamedTargetVolume, name, (volume), );
SERIALIZER_EXTERN_IMPL( aop::SceneSettings::NamedModelGroup, name, (models), );

SERIALIZER_DEFAULT_EXTERN_IMPL( aop::SceneSettings,
	(views)
	(volumes)
	(modelGroups)
	(neighborhoodDatabase_queryTolerance)
	(neighborhoodDatabase_maxDistance)
	(probeGenerator_maxDistance)
	(probeGenerator_resolution)
	(probeQuery_occlusionTolerance)
	(probeQuery_distanceTolerance)
	(probeQuery_colorTolerance)
);

SERIALIZER_DEFAULT_EXTERN_IMPL( aop::Settings,
	(scenePath)
	(sceneSettingsPath)
	(probeDatabasePath)
	(modelDatabasePath)
	(neighborhoodDatabaseV2Path)
	(validation_neighborhood_positionVariance)
	(validation_neighborhood_numSamples)
);