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

		static const int MAX_NUM_ENTRIES = 512;

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
					y += entry.renderText.getLocalBounds().height;
				}
				totalHeight = y;
			}
		}

		void renderEntries( const float maxHeightPercentage, bool drawBackground ) {
			const sf::Vector2i windowSize( application->mainWindow.getSize() );

			const float height = std::min<float>( totalHeight, maxHeightPercentage * windowSize.y );

			if( drawBackground ) {
				sf::RectangleShape background;
				background.setPosition( 0.0, 0.0 );
				background.setSize( sf::Vector2f( windowSize.x, height ) );
				background.setFillColor( sf::Color( 20, 20, 20, 128 ) );
				application->mainWindow.draw( background );
			}
			
			float y = height;
			for( auto entry = entries.rbegin() ; entry != entries.rend() ; ++entry ) {
				auto &renderText = entry->renderText;
				const auto bounds = renderText.getLocalBounds();
				y -= bounds.height;

				renderText.setPosition( -bounds.left, y - bounds.top );
				application->mainWindow.draw( renderText );

				if( y < 0 ) {
					break;
				}
			}
		}

		void renderAsNotifications() {			
			renderEntries( 0.3, true );
		}

		void renderAsLog() {
			renderEntries( 0.9, false );
		}
	};
}