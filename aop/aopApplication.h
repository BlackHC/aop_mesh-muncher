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
#include "candidateFinderInterface.h"
#include "editor.h"

namespace aop {
	struct Application {
		sf::RenderWindow mainWindow;

		Editor editor;

		EventSystem eventSystem;
		EventDispatcher eventDispatcher;
		AntTweakBarEventHandler antTweakBarEventHandler;

		WidgetRoot widgetRoot;

		Camera mainCamera;
		CameraInputControl mainCameraInputControl;

		std::unique_ptr< SGSInterface::World > world;
		SGSInterface::View cameraView;

		Settings settings;

		ProbeDatabase candidateFinder;

		Application() {}

		struct MainUI;
		struct TargetVolumesUI;
		struct CameraViewsUI;
		struct ModelTypesUI;
		struct CandidateSidebar;
		struct NamedVolumesEditorView;

		std::unique_ptr< MainUI > mainUI;
		std::unique_ptr< CameraViewsUI > cameraViewsUI;
		std::unique_ptr< TargetVolumesUI > targetVolumesUI;
		std::unique_ptr< ModelTypesUI > modelTypesUI;

		std::unique_ptr< NamedVolumesEditorView > namedVolumesEditorView;
		std::unique_ptr< CandidateSidebar > candidateSidebar;

		void init();

		void initEditor();
		void initCamera();
		void initMainWindow();
		void initSGSInterface();
		void initEventHandling();

		void initUI();
		void updateUI();

		void eventLoop();
	};
}
