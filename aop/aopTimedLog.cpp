#include "aopTimedLog.h"

static __declspec( thread ) bool isThisTheMainThread = false;

namespace aop {
	void TimedLog::init() {
		isThisTheMainThread = true;

		notifyApplicationOnMessage = false;
		rebuiltNeeded = false;
		totalHeight = 0.0;

		beginEntry = endEntry = 0;

		entries.resize( MAX_NUM_ENTRIES );

		Log::addSink(
			[this] ( int scope, const std::string &message, Log::Type type ) -> bool {
				if( this->size() >= MAX_NUM_ENTRIES ) {
					++beginEntry;
				}
				Entry &entry = getEntry( endEntry );
				entry.timeStamp = application->clock.getElapsedTime().asSeconds();

				entry.renderText.setCharacterSize( 15 );
				entry.renderText.setString( Log::Utility::indentString( scope, message, boost::str( boost::format( "[%s] " ) % entry.timeStamp ) ) );
			
				// some fancy style changes depending on the event type
				switch( type ) {
				case Log::T_ERROR:
					entry.renderText.setStyle( sf::Text::Bold );
					entry.renderText.setColor( sf::Color( 255, 0, 0 ) );
					break;
				default:
					entry.renderText.setStyle( 0 );
					entry.renderText.setColor( sf::Color( 255, 255, 255 ) );
					break;
				}

				++endEntry;

				rebuiltNeeded = true;

				if( notifyApplicationOnMessage && isThisTheMainThread ) {
					application->updateProgress();
				}

				return true;
			}
		);
	}

	void TimedLog::updateTime( float elapsedTime ) {
		const float timeOutDuration = 5.0;

		while( size() > 0 && getEntry( beginEntry ).timeStamp < elapsedTime - timeOutDuration ) {
			++beginEntry;
			rebuiltNeeded = true;
		}
	}

	void TimedLog::updateText() {
		if( rebuiltNeeded ) {
			rebuiltNeeded = false;

			float y = 0;
			for( auto entryIndex = beginEntry ; entryIndex < endEntry ; ++entryIndex ) {
				Entry &entry = getEntry( entryIndex );
				y += entry.renderText.getLocalBounds().height;
			}
			totalHeight = y;
		}
	}

	void TimedLog::renderEntries( const float maxHeightPercentage, bool drawBackground ) {
		const sf::Vector2i windowSize( application->mainWindow.getSize() );

		const float height = std::min<float>( totalHeight, maxHeightPercentage * windowSize.y );

		if( drawBackground ) {
			sf::RectangleShape background;
			background.setPosition( 0.0, 0.0 );
			background.setSize( sf::Vector2f( (float) windowSize.x, height ) );
			background.setFillColor( sf::Color( 20, 20, 20, 128 ) );
			application->mainWindow.draw( background );
		}

		float y = height;
		for( auto entryIndex = endEntry - 1 ; entryIndex >= beginEntry ; --entryIndex ) {
			auto &entry = getEntry( entryIndex );

			auto &renderText = entry.renderText;
			const auto bounds = renderText.getLocalBounds();
			y -= bounds.height;

			renderText.setPosition( -bounds.left, y - bounds.top );
			application->mainWindow.draw( renderText );

			if( y < 0 ) {
				break;
			}
		}
	}
}

