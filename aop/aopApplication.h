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

#include "editor.h"
#include <deque>
#include <logger.h>

#include <memory>

namespace aop {
	struct CandidateSidebarUI;
	struct TargetVolumesUI;
	struct CameraViewsUI;
	struct ModelTypesUI;
	struct TimedLog;

	struct Application {
		sf::Clock frameClock, clock;

		sf::RenderWindow mainWindow;

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

		ProbeDatabase candidateFinder;
		NeighborhoodDatabase neighborDatabase;
		NeighborhoodDatabaseV2 neighborDatabaseV2;

		Application() {}

		struct MainUI;
		struct NamedVolumesEditorView;

		std::shared_ptr< MainUI > mainUI;
		std::shared_ptr< CameraViewsUI > cameraViewsUI;
		std::shared_ptr< TargetVolumesUI > targetVolumesUI;
		std::shared_ptr< ModelTypesUI > modelTypesUI;

		std::shared_ptr< NamedVolumesEditorView > namedVolumesEditorView;
		std::shared_ptr< CandidateSidebarUI > candidateSidebarUI;

		void init();

		void initEditor();
		void initCamera();
		void initMainWindow();
		void initSGSInterface();
		void initEventHandling();

		void initUI();
		void updateUI();

		void eventLoop();
				
		std::shared_ptr<TimedLog> timedLog;

		void updateProgress();

		void startLongOperation();
		void endLongOperation();
	};
}
