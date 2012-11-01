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
);

SERIALIZER_DEFAULT_EXTERN_IMPL( aop::Settings,
	(scenePath)
	(sceneSettingsPath)
	(probeDatabasePath)
	(modelDatabasePath)
	(neighborhoodDatabaseV2Path)
);

namespace aop {
	static const char *settingsFilename = "aopSettings.wml";

	void Settings::load() {
		Serializer::TextReader reader( settingsFilename );
		Serializer::read( reader, *this );
	}

	void Settings::store() const {
		Serializer::TextWriter writer( settingsFilename );
		Serializer::write( writer, *this );
	}

	void SceneSettings::load( const char *filename ) {
		Serializer::TextReader reader( filename );
		Serializer::get( reader, *this );
	}

	void SceneSettings::store( const char *filename ) const {
		Serializer::TextWriter writer( filename );
		Serializer::put( writer, *this );
	}

	Settings::Settings()
		: scenePath( "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene" )
		, sceneSettingsPath( "aopSceneSettings.wml" )
		, probeDatabasePath( "probeDatabase" )
		, modelDatabasePath( "modelDatabase" )
		, neighborhoodDatabaseV2Path( "neighborhoodDatabaseV2" )
	{
	}
}