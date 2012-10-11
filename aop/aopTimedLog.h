#pragma once

#include "aopApplication.h"

#include "SFML/Graphics.hpp"

namespace aop {
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

		TimedLog( Application *application ) : application( application ) {
			init();
		}

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
}