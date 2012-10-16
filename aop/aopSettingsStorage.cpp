#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include <serializer.h>

#include "aopSettings.h"

SERIALIZER_DEFAULT_EXTERN_IMPL( Obb, (transformation)(size) );

SERIALIZER_EXTERN_IMPL( aop::Settings::NamedCameraState, name, (position)(direction), );
SERIALIZER_EXTERN_IMPL( aop::Settings::NamedTargetVolume, name, (volume), );
SERIALIZER_EXTERN_IMPL( aop::Settings::NamedModelGroup, name, (models), );

SERIALIZER_DEFAULT_EXTERN_IMPL( aop::Settings, 
	(views)
	(volumes)
	(modelGroups)
	(neighborhoodDatabase_queryTolerance)
	(neighborhoodDatabase_maxDistance)
	(probeGenerator_maxDistance)
	(probeGenerator_resolution)
);

namespace aop {
	static const char *settingsFilename = "aopSettings.wml";

	void Settings::load() {
		Serializer::TextReader reader( settingsFilename );
		Serializer::get( reader, *this );
	}

	void Settings::store() const {
		Serializer::TextWriter writer( settingsFilename );
		Serializer::put( writer, *this );
	}
}