#pragma once

#include <vector>
#include <SFML/Window.hpp>

struct EventHandler {
	// returns true if the event has been processed
	virtual bool handleEvent( const sf::Event &event ) { return false; }
	virtual bool update( const float elapsedTime, bool inputProcessed ) { return inputProcessed; }
};

struct EventDispatcher : public EventHandler {
	std::vector<std::shared_ptr<EventHandler>> eventHandlers;

	bool handleEvent( const sf::Event &event ) {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			if( eventHandler->get()->handleEvent( event ) ) {
				return true;
			}
		}
		return false;
	}

	bool update( const float elapsedTime, bool inputProcessed = false ) {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			inputProcessed |= eventHandler->get()->update( elapsedTime, inputProcessed );
		}
		return inputProcessed;
	}
};