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
#include "neighborhoodDatabase.h"

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
		NeighborhoodDatabase neighborDatabase;
		NeighborhoodDatabaseV2 neighborDatabaseV2;

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
			Application *application;

			struct Entry {
				float timeStamp;
				sf::Text renderText;
				Entry() : timeStamp() {}
			};
			std::vector< Entry > entries;

			static const int MAX_NUM_ENTRIES = 32;

			template< int limit >
			struct CycleCounter {
				int value;

				CycleCounter() : value() {}
				CycleCounter( const CycleCounter &other ) : value( other.value ) {}

				void operator ++ () {
					value = (value + 1) % limit;
				}

				operator int () const {
					return value;
				}
			};
			// TODO: dont use a cycle counter - only use it during access to be able to check whether the buffer is empty or not [10/9/2012 kirschan2]
			CycleCounter< MAX_NUM_ENTRIES > beginEntry, endEntry;

			int size() const {
				if( endEntry < beginEntry ) {
					return endEntry + MAX_NUM_ENTRIES - beginEntry;
				}
				return endEntry - beginEntry;
			}

			float totalHeight;

			bool rebuiltNeeded;

			bool notifyApplicationOnMessage;

			TimedLog( Application *application );

			void init();

			// TODO: remove elapsed time and use application->clock [10/9/2012 kirschan2]
			void updateTime( float elapsedTime ) {
				const float timeOutDuration = 5.0;

				while( size() != 0 && entries[ beginEntry ].timeStamp < elapsedTime - timeOutDuration ) {
					++beginEntry;
					rebuiltNeeded = true;
				}
			}

			void updateText() {
				if( rebuiltNeeded ) {
					rebuiltNeeded = false;

					float y = 0;
					for( auto entryIndex = beginEntry ; entryIndex != endEntry ; ++entryIndex ) {
						Entry &entry = entries[ entryIndex ];
						entry.renderText.setPosition( 0.0, y );
						y += entry.renderText.getLocalBounds().height;
					}
					totalHeight = y;
				}
			}

			void renderEntries() {
				for( auto entryIndex = beginEntry ; entryIndex != endEntry ; ++entryIndex ) {
					const Entry &entry = entries[ entryIndex ];
					application->mainWindow.draw( entry.renderText );
				}
			}

			void renderAsNotifications() {
				const sf::Vector2i windowSize( application->mainWindow.getSize() );

				sf::RectangleShape background;
				background.setPosition( 0.0, 0.0 );
				background.setSize( sf::Vector2f( windowSize.x, totalHeight ) );
				background.setFillColor( sf::Color( 20, 20, 20, 128 ) );
				application->mainWindow.draw( background );

				renderEntries();
			}

			void renderAsLog() {
				const sf::Vector2i windowSize( application->mainWindow.getSize() );

				sf::View view = application->mainWindow.getView();
				sf::View shiftedView = view;
				shiftedView.move( 0.0, -(windowSize.y * 0.9 - totalHeight) );
				application->mainWindow.setView( shiftedView );

				renderEntries();

				application->mainWindow.setView( view );
			}
		};
		std::unique_ptr<TimedLog> timedLog;

		void updateProgress();

		void startLongOperation();
		void endLongOperation();
	};
}
