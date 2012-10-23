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

		int beginEntry, endEntry;

		int size() const {
			return endEntry - beginEntry;
		}

		float totalHeight;

		bool rebuiltNeeded;

		bool notifyApplicationOnMessage;

		TimedLog( Application *application ) 
			: application( application )
			, beginEntry( 0 )
			, endEntry( 0 )
		{
			init();
		}

		Entry &getEntry( int index ) {
			return entries[ index % MAX_NUM_ENTRIES ];
		}

		const Entry &getEntry( int index ) const {
			return entries[ index % MAX_NUM_ENTRIES ];
		}

		void init();

		// TODO: remove elapsed time and use application->clock [10/9/2012 kirschan2]
		void updateTime( float elapsedTime );

		void updateText();

		void renderEntries( const float maxHeightPercentage, bool drawBackground );

		void renderAsNotifications() {
			renderEntries( 0.3f, true );
		}

		void renderAsLog() {
			renderEntries( 0.9f, false );
		}
	};
}