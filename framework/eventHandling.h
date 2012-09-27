#pragma once

#include <vector>
#include <SFML/Window.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include "boost/range/adaptor/reversed.hpp"

/* 3 types of events:
 * window events:
 *	closed
 *	resized
 *	lost focus
 *	gained focus
 *	mouse entered/left
 *	
 *	these events are global and all event handlers are notified of them
 *	
 * mouse events:
 *	wheel moved
 *	button pressed/released
 *	mouse move
 *		
 *	one object can decide to capture all these events exclusively (input capture, which can hide the cursor and captures the keyboard, too)
 *	
 *	otherwise unpressed mouse move events are passed down until one object decides to capture the event
 *	other events (including keyboard) are offered to this object first afterwards, then they are passed up to its parent for it to handle them somehow if possible
 *	this can involve passing them down again to other children
 *	
 *	
 * keyboard events:
 *	text entered
 *	key pressed
 *	key released
 *	
 *	one object can ask to capture all keyboard events
 *	
 * joystick events are ignored
 *
 * virtual events:
 *	refresh/update
 *	lost/gained keyboard/mouse focus
 *	lost/gained exclusive mode
 */

enum FocusType {
	FT_NONE		= 0,
	FT_KEYBOARD	= 1,
	FT_MOUSE	= 2,
	FT_BOTH		= 3,
	FT_EXCLUSIVE= 4,
	FT_ALL		= 7
};

inline bool isGlobalEvent( sf::Event::EventType type ) {
	switch( type ) {
	case sf::Event::Closed:
	case sf::Event::Resized:
	case sf::Event::GainedFocus:
	case sf::Event::LostFocus:
	case sf::Event::MouseEntered:
	case sf::Event::MouseLeft:
		return true;
	default:
		return false;
	}
}

inline bool isMouseInputEvent( sf::Event::EventType type ) {
	switch( type ) {
	case sf::Event::MouseMoved:
	case sf::Event::MouseWheelMoved:
	case sf::Event::MouseButtonPressed:
	case sf::Event::MouseButtonReleased:
		return true;
	default:
		return false;
	}
}

inline bool isKeyboardInputEvent( sf::Event::EventType type ) {
	switch( type ) {
	case sf::Event::TextEntered:
	case sf::Event::KeyPressed:
	case sf::Event::KeyReleased:
		return true;
	default:
		return false;
	}
}

struct EventHandler;
typedef std::shared_ptr<EventHandler> EventHandlerPtr;
// the actual handler is at the end
typedef std::vector< EventHandlerPtr > EventHandlers;

struct EventState {
	const sf::Event &event;
	// make sure you never call this handler
	EventHandler *previousHandler;

	virtual void setCapture( EventHandler *handler, FocusType focusType ) const = 0;
	virtual FocusType getCapture( const EventHandler *handler ) const = 0;

	// accept the event ie we have handled it
	virtual void accept() = 0;
	virtual bool hasAccepted() const = 0;
		
	EventState( const sf::Event &event ) : event( event ), previousHandler( nullptr ) {}

	bool isExclusive( const EventHandler *handler ) const {
		return getCapture( handler ) & FT_EXCLUSIVE;
	}
};

struct EventSystem;

struct EventHandler {
	EventHandler *parent;

	// global event
	virtual void onNotify( const EventState &eventState ) = 0;

	virtual void onKeyboard( EventState &eventState ) = 0;
	virtual void onMouse( EventState &eventState ) = 0;

	// virtual events: gain/lose focus
	virtual bool acceptFocus( FocusType focusType ) = 0;

	virtual void loseFocus( FocusType focusType ) = 0;
	virtual void gainFocus( FocusType focusType ) = 0;

	// virtual update event
	virtual void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) = 0;

	EventHandler( EventHandler *parent = nullptr ) : parent( parent ) {}

	// TODO: add a wrapper template class and derive from EventHandler
	// additional, optional event handlers
	virtual void onSelected() {};
	virtual void onUnselected() {};

	virtual std::string getHelp( const std::string &prefix = std::string() ) { return std::string(); }
};

struct NullEventHandler : EventHandler {
	virtual void onNotify( const EventState &eventState )  {
	}

	virtual void onKeyboard( EventState &eventState )  {
	}

	virtual void onMouse( EventState &eventState )  {
	}

	virtual bool acceptFocus( FocusType focusType )  {
		return false;
	}

	virtual void loseFocus( FocusType focusType )  {
	}

	virtual void gainFocus( FocusType focusType )  {
	}

	virtual void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime )  {
	}
};

struct ExclusiveMode {
	sf::Vector2i mouseDelta;
	sf::Vector2i mouseAbsolute;

	std::shared_ptr< sf::Window > window;

	bool active;

	ExclusiveMode() : active( false ) {}

	sf::Vector2i popMouseDelta() {
		const sf::Vector2i temp = mouseDelta;
		mouseDelta = sf::Vector2i();
		return temp;
	}

	void enter() {
		if( active ) {
			return;
		}

		savedMousePosition = sf::Mouse::getPosition( *window );
		mouseAbsolute = mouseDelta = sf::Vector2i();
		
		window->setMouseCursorVisible( false );
			
		centerCursor();

		active = true;
		ignoreNextMouseMove = true;
	}

	void leave() {
		if( active ) {
			sf::Mouse::setPosition( savedMousePosition, *window );
			window->setMouseCursorVisible( true );
			active = false;
		}
	}
	
	void centerCursor() {
		sf::Mouse::setPosition( sf::Vector2i( window->getSize() ) / 2, *window );
		lastMousePosition = sf::Mouse::getPosition();
	}

	bool cursorWantsReset() const {
		const sf::Vector2i relativeCursorPosition = sf::Mouse::getPosition( *window );
		const sf::Vector2i windowCenter( window->getSize() / 2u );
		const sf::Vector2i borderSize( window->getSize() / 4u );

		const sf::Vector2i delta = relativeCursorPosition - windowCenter;
		if( delta.x < -borderSize.x || 
			delta.x > borderSize.x || 
			delta.y < -borderSize.y || 
			delta.y > borderSize.y 
		) {
			return true;
		}
		return false;
	}

	// return true if the event has been fully processed internally
	bool onEvent( sf::Event &event ) {
		if( event.type == sf::Event::MouseMoved ) {
			if( ignoreNextMouseMove ) {
				 ignoreNextMouseMove = false;
				return true;
			}

			const sf::Vector2i currentMousePosition = sf::Mouse::getPosition();
			const sf::Vector2i localMouseDelta = currentMousePosition - lastMousePosition;
			mouseDelta += localMouseDelta; 
			mouseAbsolute += localMouseDelta;
			
			// check if we are near the border
			if( cursorWantsReset() ) {
				ignoreNextMouseMove = true;

				centerCursor();
			}
			else {
				lastMousePosition = currentMousePosition;
			}
		}
		else if( event.type == sf::Event::MouseLeft || event.type == sf::Event::MouseEntered ) {
			if( ignoreNextMouseMove ) {
				ignoreNextMouseMove = false;
				return true;
			}

			ignoreNextMouseMove = true;
			centerCursor();

			return true;
		}

		if( event.type == sf::Event::MouseMoved ) {
			event.mouseMove.x = mouseAbsolute.x;
			event.mouseMove.y = mouseAbsolute.y;
		}
		else if( event.type == sf::Event::MouseWheelMoved ) {
			event.mouseWheel.x = mouseAbsolute.x;
			event.mouseWheel.y = mouseAbsolute.y;
		}
		else if( event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::MouseButtonReleased ) {
			event.mouseButton.x = mouseAbsolute.x;
			event.mouseButton.y = mouseAbsolute.y;
		}
		return false;
	}

private:
	bool ignoreNextMouseMove;
	
	sf::Vector2i savedMousePosition;
	sf::Vector2i lastMousePosition;
};

struct EventSystem {
	EventHandler *keyboardFocusHandler, *mouseFocusHandler;
	EventHandler *exclusiveHandler;

	EventHandlerPtr rootHandler;

	ExclusiveMode exclusiveMode;

	struct EventStateImpl : ::EventState {
		const sf::Event event;

		EventSystem *eventSystem;
		bool accepted;

		EventHandler *caller;

		EventStateImpl( EventSystem *eventSystem, const sf::Event &event ) 
			: eventSystem( eventSystem ), EventState( event ), accepted( false ) {}

		void accept() {
			accepted = true;
		}

		bool hasAccepted() const {
			return accepted;
		}

		void setCapture( EventHandler *handler, FocusType focusType ) const {
			// TODO: make this delayed? [9/27/2012 kirschan2]
			eventSystem->setCapture( handler, focusType );
		}

		FocusType getCapture( const EventHandler *handler ) const {
			return eventSystem->getCapture( handler );
		}
	};

	EventSystem() : keyboardFocusHandler( nullptr ), mouseFocusHandler( nullptr ), exclusiveHandler( nullptr ) {}

	FocusType getCapture( const EventHandler *handler ) const {
		int focusType = 0;
		if( mouseFocusHandler == handler ) {
			focusType += FT_MOUSE;
		}
		if( keyboardFocusHandler == handler ) {
			focusType += FT_KEYBOARD;
		}
		if( exclusiveHandler == handler ) {
			focusType += FT_EXCLUSIVE;
		}
		return FocusType( focusType );
	}

	void setMouseCapture( EventHandler *handler ) {
		if( mouseFocusHandler ) {
			mouseFocusHandler->loseFocus( FT_MOUSE );
		}
		mouseFocusHandler = handler;
		if( mouseFocusHandler ) {
			mouseFocusHandler->gainFocus( FT_MOUSE );
		}
	}

	void setKeyboardCapture( EventHandler *handler ) {
		if( keyboardFocusHandler ) {
			keyboardFocusHandler->loseFocus( FT_KEYBOARD );
		}
		keyboardFocusHandler = handler;
		if( keyboardFocusHandler ){
			keyboardFocusHandler->gainFocus( FT_KEYBOARD );
		}
	}

	void setExclusiveCapture( EventHandler *handler ) {
		if( exclusiveHandler ) {
			exclusiveHandler->loseFocus( FT_EXCLUSIVE );
		}
		exclusiveHandler = handler;
		if( exclusiveHandler ) {
			exclusiveHandler->gainFocus( FT_EXCLUSIVE );
		}

		if( exclusiveHandler ) {
			exclusiveMode.enter();
		}
		else {
			exclusiveMode.leave();
		}
	}

	void setCapture( EventHandler *handler, FocusType focusType ) {
		if( (focusType == FT_EXCLUSIVE) && (handler != exclusiveHandler) ) {
			if( !handler || handler->acceptFocus( FT_EXCLUSIVE ) ) {
				setExclusiveCapture( handler );
			}
		}
		else if( (focusType != FT_EXCLUSIVE) && (handler == exclusiveHandler) ) {
			setExclusiveCapture( nullptr );
		}

		if( (focusType & FT_MOUSE) && (handler != mouseFocusHandler) ) {
			if( !handler || handler->acceptFocus( FT_MOUSE ) ) {
				setMouseCapture( handler );
			}
		}
		else if( !(focusType & FT_MOUSE) && (handler == mouseFocusHandler) ) {
			setMouseCapture( nullptr );
		}

		if( (focusType & FT_KEYBOARD) && (handler != keyboardFocusHandler) ) {
			if( !handler || handler->acceptFocus( FT_KEYBOARD ) ) {
				setKeyboardCapture( handler );
			}
		}
		else if( !(focusType & FT_KEYBOARD) && (handler == keyboardFocusHandler) ) {
			setKeyboardCapture( nullptr );
		}
	}

	bool processEvent( const sf::Event &event ) {
		// are we in exclusive mode?
		if( exclusiveHandler ) {
			sf::Event adaptedEvent = event;
			
			if( exclusiveMode.onEvent( adaptedEvent ) ) {
				return true;
			}

			EventStateImpl eventState( this, adaptedEvent );

			// is it a global event?
			if( isGlobalEvent( adaptedEvent.type ) ) {
				rootHandler->onNotify( eventState );
				return false;
			}
			else if( isKeyboardInputEvent( adaptedEvent.type ) ) {
				exclusiveHandler->onKeyboard( eventState ); 
			}
			else if( isMouseInputEvent( adaptedEvent.type ) ){
				exclusiveHandler->onMouse( eventState );
			}
			else {
				// unknown event
			}

			return true;
		}
		else {
			EventStateImpl eventState( this, event );

			// is it a global event?
			if( isGlobalEvent( event.type ) ) {
				rootHandler->onNotify( eventState );
				return false;
			}
			// unpressed mouse move event?
			else if( 
				event.type == sf::Event::MouseMoved &&
				!sf::Mouse::isButtonPressed(sf::Mouse::Left) &&
				!sf::Mouse::isButtonPressed(sf::Mouse::Right) &&
				!sf::Mouse::isButtonPressed(sf::Mouse::Middle)
			) {
				// try to get a new focus object

				rootHandler->onMouse( eventState );
				
				if( eventState.accepted ) {
					return true;
				}

				// otherwise fall through
			}
			else if( isMouseInputEvent( event.type ) ) {
				// try the focus handler
				if( mouseFocusHandler ) {
					EventHandler *currentHandler = mouseFocusHandler;

					do {
						currentHandler->onMouse( eventState );
					
						if( eventState.accepted ) {
							return true;
						}

						// try the parent
						EventHandler *parentHandler = currentHandler->parent;
						if( !parentHandler ) {
							break;
						}
						eventState.previousHandler = currentHandler;
						currentHandler = parentHandler;
					} while( true );

					// fall through and try again with root
				}
								
				eventState.previousHandler = nullptr;
				rootHandler->onMouse( eventState );
			}
			else if( isKeyboardInputEvent( event.type ) ) {
				// try the focus handler
				if( mouseFocusHandler ) {
					EventHandler *currentHandler = mouseFocusHandler;

					do {
						currentHandler->onKeyboard( eventState );

						if( eventState.accepted ) {
							return true;
						}

						// try the parent
						EventHandler *parentHandler = currentHandler->parent;
						if( !parentHandler ) {
							break;
						}
						eventState.previousHandler = currentHandler;
						currentHandler = parentHandler;
					} while( true );

					// fall through and try again with root
				}

				eventState.previousHandler = nullptr;
				rootHandler->onKeyboard( eventState );
			}
			else {
				// unknown event
			}

			return eventState.accepted;
		}
	}

	void update( const float frameDuration, const float elapsedTime ) {
		rootHandler->onUpdate( *this, frameDuration, elapsedTime );
	}
};

struct EventDispatcher : EventHandler {
	std::vector<std::shared_ptr<EventHandler>> eventHandlers;

	const char *name;

	EventDispatcher( const char *name ) : name( name ) {}

	void addEventHandler( const std::shared_ptr<EventHandler> &handler ) {
		eventHandlers.push_back( handler );
		handler->parent = this;
	}

	void onNotify( const EventState &eventState )  {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			eventHandler->get()->onNotify( eventState );
		}
	}

	void onKeyboard( EventState &eventState )  {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() && !eventState.hasAccepted() ; ++eventHandler ) {
			eventHandler->get()->onKeyboard( eventState );
		}
	}

	void onMouse( EventState &eventState )  {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() && !eventState.hasAccepted() ; ++eventHandler ) {
			eventHandler->get()->onMouse( eventState );
		}
	}

	bool acceptFocus( FocusType focusType )  {
		if( focusType == FT_EXCLUSIVE ) {
			return false;
		}
		return true;
	}

	void loseFocus( FocusType focusType )  {
	}

	void gainFocus( FocusType focusType )  {
	}

	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime )  {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			eventHandler->get()->onUpdate( eventSystem, frameDuration, elapsedTime );
		}
	}

	std::string getHelp( const std::string &prefix = std::string() ) {
		const std::string subPrefix = prefix + "\t";
		return 
			prefix + name + "\n" +
			boost::join( 
				eventHandlers | 
					boost::adaptors::reversed |
					boost::adaptors::transformed( std::bind( &EventHandler::getHelp, std::placeholders::_1, subPrefix ) )
				,
				"" 
			)
		;
	}
};

struct EventRouter : EventHandler {
	std::vector<std::shared_ptr<EventHandler>> eventHandlers;
	EventHandler *target;

	const char *name;

	EventRouter( const char *name ) : name( name ) {}

	void addEventHandler( const std::shared_ptr<EventHandler> &handler ) {
		eventHandlers.push_back( handler );
		handler->parent = this;
	}

	void onNotify( const EventState &eventState )  {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			eventHandler->get()->onNotify( eventState );
		}
	}

	void onKeyboard( EventState &eventState )  {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() && !eventState.hasAccepted() ; ++eventHandler ) {
			if( eventHandler->get() == target ) {
				eventHandler->get()->onKeyboard( eventState );
			}
		}
	}

	void onMouse( EventState &eventState )  {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() && !eventState.hasAccepted() ; ++eventHandler ) {
			if( eventHandler->get() == target ) {
				eventHandler->get()->onMouse( eventState );
			}
		}
	}

	bool acceptFocus( FocusType focusType )  {
		if( focusType == FT_EXCLUSIVE ) {
			return false;
		}
		return true;
	}

	void loseFocus( FocusType focusType )  {
	}

	void gainFocus( FocusType focusType )  {
	}

	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime )  {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			eventHandler->get()->onUpdate( eventSystem, frameDuration, elapsedTime );
		}
	}

	void setTarget( EventHandler *handler ) {
		if( target ) {
			target->onUnselected();
		}
		target = handler;
		if( target ) {
			target->onSelected();
		}
	}

	std::string getHelp( const std::string &prefix = std::string() ) {
		const std::string subPrefix = prefix + "\t";

		for( auto eventHandler = eventHandlers.begin() ; eventHandler != eventHandlers.end() ; ++eventHandler ) {
			if( eventHandler->get() == target ) {
				return prefix + name + "\n" +
					boost::join( 
						eventHandlers |
							boost::adaptors::reversed |
							boost::adaptors::transformed( std::bind( &EventHandler::getHelp, std::placeholders::_1, subPrefix ) )
						,
						""
					)
				;
			}
		}
		return prefix + name + ": inactive\n";
	}
};