#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "probeDatabase.h"

#include "queryResultStorage.h"

/*
namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, type &value ) {

	}
	template< typename Writer > \
	void write( Writer &writer, const type &value ) {

	}
}*/

BOOST_STATIC_ASSERT( sizeof( ProbeContext::RawProbe ) == 4 );
BOOST_STATIC_ASSERT( sizeof( ProbeContext::RawProbeSample ) == 8 );
BOOST_STATIC_ASSERT( sizeof( ProbeContext::DBProbeSample ) == 8 + 8 );

SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::SampledModel::SampledInstance, (sourceTransformation)(probeSamples) )
SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::IndexedProbeSamples, (data)(occlusionLowerBounds) )

SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::ColorCounter,
	(buckets)
	(totalNumSamples)
	(entropy)
	(totalMessageLength)
	(globalMessageLength)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::SampledModel,
	(instances)
	(mergedInstances)
	(mergedInstancesByDirectionIndex)
	(probes)
	(rotatedProbePositions)
	(resolution)
	(modelColorCounter)

	(sampleBitPlane)
	(linearizedProbeSamples)
	(sampleProbeIndexMapByDirection)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::LinearizedProbeSamples,
	(numProbes)
	(samples)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::SampleProbeIndexMap::SampleMultiMap,
	(items)
	(bucketOffsets)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::SampleProbeIndexMap,
	(sampleMultiMap)
)

SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::SampleQuantizer,
	(maxDistance)
)

// TODO: add forward friends [11/11/2012 Andreas]
SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::ProbeDatabase,
	(sampledModels)
	(globalColorCounter)
	(sampleQuantizer)
	(localModelNames)
)

SERIALIZER_ENABLE_RAW_MODE_EXTERN( ProbeContext::SampleBitPlane );

SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::ProbeSample );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( ProbeContext::RawProbe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( ProbeContext::DBProbeSample );

// TODO: this is a duplicate from aopSettingsStorage.cpp---add a storage header instead? [10/22/2012 kirschan2]
SERIALIZER_DEFAULT_EXTERN_IMPL( Obb, (transformation)(size) );