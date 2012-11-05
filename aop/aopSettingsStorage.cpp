#include "aopSettingsStorage.h"

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
		, neighborhoodValidationDataPath( "neighborhood.validationData" )
	{
	}
}