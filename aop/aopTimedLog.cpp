#include "aopTimedLog.h"

void aop::TimedLog::init() {
	notifyApplicationOnMessage = false;
	rebuiltNeeded = false;
	totalHeight = 0.0;

	entries.resize( MAX_NUM_ENTRIES );

	Log::addSink(
		[this] ( int scope, const std::string &message, Log::Type type ) -> bool {
			if( this->size() >= MAX_NUM_ENTRIES - 1 ) {
				++beginEntry;
			}
			Entry &entry = entries[endEntry];
			entry.timeStamp = application->clock.getElapsedTime().asSeconds();

			entry.renderText.setCharacterSize( 15 );
			entry.renderText.setString( Log::Utility::indentString( scope, message, boost::str( boost::format( "[%s] " ) % entry.timeStamp ) ) );
			
			++endEntry;

			rebuiltNeeded = true;

			if( notifyApplicationOnMessage ) {
				application->updateProgress();
			}

			return true;
		}
	);

	
}