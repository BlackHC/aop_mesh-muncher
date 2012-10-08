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
#include <deque>
#include <logger.h>

namespace aop {
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

		struct TimedLog {
			struct Entry {
				float timestamp;
				std::string indentedMessage;
				Entry() {}
			};
			std::deque< Entry > entries;

			bool rebuildNeeded;

			sf::Text renderText;

			float currentEntryTime;
			float currentElapsedTime;

			void init();

			void updateTime( float elapsedTime ) {
				const float timeOutDuration = 10.0;

				currentEntryTime = currentElapsedTime = elapsedTime;

				while( !entries.empty() && entries.front().timestamp < elapsedTime - timeOutDuration ) {
					entries.pop_front();
					rebuildNeeded = true;
				}
			}

			void updateText() {
				if( rebuildNeeded ) {
					rebuildNeeded = false;

					std::string mergedEntries;
					for( auto entry = entries.begin() ; entry != entries.end() ; ++entry ) {
						mergedEntries += entry->indentedMessage;
					}

					renderText.setString( mergedEntries );
				}
			}
		};
		TimedLog timedLog;

		void updateProgress( float percentage );
	};
}
