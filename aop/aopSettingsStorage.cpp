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

	void SceneSettings::load( const std::string &filename ) {
		Serializer::TextReader reader( filename );
		Serializer::get( reader, *this );
	}

	void SceneSettings::store( const std::string &filename ) const {
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
		, validation_neighborhood_positionVariance()
		, validation_neighborhood_numSamples(1)

		, probeValidationDataPath( "probe.validationData" )
		, validation_probes_positionVariance()
		, validation_probes_numSamples(1)
		, validation_probes_queryVolumeSize(3)
	{
	}
}