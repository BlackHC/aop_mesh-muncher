#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "queryResult.h"

SERIALIZER_DEFAULT_EXTERN_IMPL( QueryResult,
	(score)
	(sceneModelIndex)
	(transformation)
)
