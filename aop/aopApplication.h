#pragma once

#include <eventHandling.h>
#include <camera.h>
#include <cameraInputControl.h>
#include <antTweakBarEventHandler.h>

#include "widgets.h"
#include "modelButtonWidget.h"
#include "sgsInterface.h"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "aopSettings.h"

#include "probeDatabase.h"
#include "neighborhoodDatabase.h"
#include "modelDatabase.h"

#include "editor.h"
#include <deque>
#include <logger.h>

#include <debugWindows.h>

#include <memory>

struct DebugUI;

namespace aop {
	enum NormalGenerationMode {
		NGM_POSITION,
		NGM_AVERAGE_NORMAL,
		NGM_NEIGHBORS,
		NGM_COMBINED
	};

	struct CandidateSidebarUI;
	struct LocalCandidateBarUI;
	struct ModelSelectionBarUI;

	struct TargetVolumesUI;
	struct CameraViewsUI;
	struct ModelTypesUI;
	struct TimedLog;

	struct ModelDatabaseUI;
	struct ProbeDatabaseUI;

	namespace DebugObjects {
		struct SceneDisplayListObject;

		struct SGSRenderer;
		struct OptixView;
		struct ProbeDatabase;
		struct ModelDatabase;
	}

	struct Application : ModelDatabase::ImportInterface {
		enum QueryType {
			QT_NORMAL,
			QT_IMPORTANCE,
			QT_FULL,
			QT_IMPORTANCE_FULL,
			QT_FAST_QUERY,
			QT_FAST_IMPORTANCE,
			QT_FAST_FULL
		};

		enum MeasureType {
			MT_NORMAL,
			MT_IMPORTANCE_WEIGHTED,
			MT_JACCARD
		};

		sf::Clock frameClock, clock;

		sf::RenderWindow mainWindow;

		DebugWindowManager debugWindowManager;

		Editor editor;

		EventSystem eventSystem;
		EventDispatcher eventDispatcher;
		AntTweakBarEventHandler antTweakBarEventHandler;

		WidgetRoot widgetRoot;

		Camera mainCamera;
		CameraInputControl mainCameraInputControl;

		std::shared_ptr< SGSInterface::World > world;
		SGSInterface::View cameraView;

		Settings settings;
		SceneSettings sceneSettings;
		bool hideScene;
		// TODO: note this is all that has been implemented so far [11/8/2012 kirschan2]
		bool hideBottomBar;

		ProbeContext::ProbeDatabase probeDatabase;
		Neighborhood::NeighborhoodDatabaseV2 neighborDatabaseV2;
		ModelDatabase modelDatabase;

		bool renderOptixView;

		Application()
			: renderOptixView( false )
			, modelDatabase( this )
			, hideScene( false )
			, hideBottomBar( true )
		{}

		struct MainUI;
		struct NamedVolumesEditorView;

		std::shared_ptr< MainUI > mainUI;
		std::shared_ptr< CameraViewsUI > cameraViewsUI;
		std::shared_ptr< TargetVolumesUI > targetVolumesUI;
		std::shared_ptr< ModelTypesUI > modelTypesUI;

		std::shared_ptr< NamedVolumesEditorView > namedVolumesEditorView;
		std::shared_ptr< CandidateSidebarUI > candidateSidebarUI;
		std::shared_ptr< ModelSelectionBarUI > modelSelectionBarUI;

		std::shared_ptr< ModelDatabaseUI > modelDatabaseUI;
		std::shared_ptr<TimedLog> timedLog;

		std::shared_ptr< DebugUI > debugUI;
		std::shared_ptr< DebugObjects::ProbeDatabase > probeDatabase_debugUI;

		std::vector< std::shared_ptr< LocalCandidateBarUI > > localCandidateBarUIs;

		void removeLocalCandidateBarUI( LocalCandidateBarUI *localCandidateBarUIPtr ) {
			for( auto localCandidateBarUI = localCandidateBarUIs.begin() ; localCandidateBarUI != localCandidateBarUIs.end() ; ++localCandidateBarUI ) {
				if( localCandidateBarUI->get() == localCandidateBarUIPtr ) {
					localCandidateBarUIs.erase( localCandidateBarUI );
					break;
				}
			}
		}

		void init();

		void initEditor();
		void initCamera();
		void initMainWindow();
		void initSGSInterface();
		void initEventHandling();

		void initUI();
		void updateUI();

		void eventLoop();

		// TODO: move these 3 functions into their own object? [10/14/2012 kirschan2]
		void updateProgress();

		void startLongOperation();
		void endLongOperation();

		void ModelDatabase_init();
		void ModelDatabase_sampleAll( NormalGenerationMode normalGenerationMode );
		// returns the number of non-empty voxels
		int ModelDatabase_sampleModel( int sceneModelIndex, float resolution, NormalGenerationMode normalGenerationMode );

		virtual void sampleModel( int modelId, float resolution, ModelDatabase::ImportInterface::Tag ) {
			ModelDatabase_sampleModel( modelId, resolution, NGM_COMBINED );
		}

		QueryResults queryVolume( const SceneSettings::NamedTargetVolume &queryVolume, QueryType queryType );
		
		QueryResults normalQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples );
		QueryResults importanceQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples );
		QueryResults fullQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples );
		QueryResults importanceFullQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples );

		QueryResults fastNormalQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples );
		QueryResults fastImportanceQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples );
		QueryResults fastFullQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples );

		void ProbeDatabase_sampleInstances( int modelIndex );

		ProbeContext::ProbeContextTolerance getPCTFromSettings();

		void NeighborhoodDatabase_sampleScene( float maxDistance );
		void NeighborhoodDatabase_sampleModels( std::vector<int> modelIndices, float maxDistance );
		Neighborhood::Results NeighborhoodDatabase_queryVolume( const Obb &queryVolume, float maxDistance, MeasureType measureType );

		// validation helper
		void NeighborhoodValidation_queryAllInstances( const std::string &filename );
		void ProbesValidation_queryAllInstances( const std::string &filename );
		void ProbesValidation_determineQueryVolumeSizeForMarkedModels() const;
	};
}
