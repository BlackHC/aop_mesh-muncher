#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "probeDatabase.h"

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
SERIALIZER_DEFAULT_EXTERN_IMPL( ProbeContext::SampledModel,
	(instances)
	(mergedInstances)
	(mergedInstancesByDirectionIndex)
	(probes)
	(rotatedProbePositions)
	(resolution)
)

SERIALIZER_ENABLE_RAW_MODE_EXTERN( OptixProgramInterface::ProbeSample );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( ProbeContext::RawProbe );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( ProbeContext::DBProbeSample );
SERIALIZER_ENABLE_RAW_MODE_EXTERN( ProbeContext::QueryResult );

// TODO: this is a duplicate from aopSettingsStorage.cpp---add a storage header instead? [10/22/2012 kirschan2]
SERIALIZER_DEFAULT_EXTERN_IMPL( Obb, (transformation)(size) );