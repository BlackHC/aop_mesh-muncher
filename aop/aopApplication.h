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
	struct CandidateSidebarUI;
	struct TargetVolumesUI;
	struct CameraViewsUI;
	struct ModelTypesUI;
	struct TimedLog;
	
	struct ModelDatabaseUI;

	struct ProbeDatabaseUI;
	
	struct Application : ModelDatabase::ImportInterface {
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

		ProbeDatabase probeDatabase;
		NeighborhoodDatabase neighborDatabase;
		NeighborhoodDatabaseV2 neighborDatabaseV2;
		ModelDatabase modelDatabase;

		bool renderOptixView;

		Application() : renderOptixView( false ), modelDatabase( this ) {}

		struct MainUI;
		struct NamedVolumesEditorView;

		std::shared_ptr< MainUI > mainUI;
		std::shared_ptr< CameraViewsUI > cameraViewsUI;
		std::shared_ptr< TargetVolumesUI > targetVolumesUI;
		std::shared_ptr< ModelTypesUI > modelTypesUI;

		std::shared_ptr< NamedVolumesEditorView > namedVolumesEditorView;
		std::shared_ptr< CandidateSidebarUI > candidateSidebarUI;

		std::shared_ptr< DebugUI > debugUI;

		std::shared_ptr< ModelDatabaseUI > modelDatabaseUI;

		std::shared_ptr<TimedLog> timedLog;

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
		void ModelDatabase_sampleAll();
		int ModelDatabase_sampleModel( int modelId, float resolution );

		virtual void sampleModel( int modelId, float resolution, ModelDatabase::ImportInterface::Tag ) {
			ModelDatabase_sampleModel( modelId, resolution );
		}
		ProbeDatabase::Query::MatchInfos queryVolume( const Obb &queryVolume );
		ProbeDatabase::FullQuery::MatchInfos fullQueryVolume( const Obb &queryVolume );
		void sampleInstances( int modelIndex );
	};
}
