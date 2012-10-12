#pragma once

#include <AntTweakBar.h>
#include "eventHandling.h"
#include <memory>

int TW_CALL TwEventSFML20(const sf::Event *event);

// TODO: really support multiple contexts by linking the window to render somehow automatically or by passing it as a parameter [10/1/2012 kirschan2]
struct AntTweakBarEventHandler : public EventHandler::WithDefaultParentImpl {
	// TODO: name refactoring [10/1/2012 kirschan2]
	struct GlobalScope : std::enable_shared_from_this<GlobalScope> {
		GlobalScope() {
			TwInit( TW_OPENGL, NULL );
		}
		~GlobalScope() {
			TwTerminate();
		}
	};
	// TODO: add a source file for this [10/1/2012 kirschan2]
	static std::weak_ptr<GlobalScope> globalScope;
	std::shared_ptr<GlobalScope> myGlobalScope;

	virtual void init( sf::Window &window ) {
		myGlobalScope = globalScope.lock();
		if( !myGlobalScope ) {	
			globalScope = myGlobalScope = std::make_shared<GlobalScope>();
		}

		const auto size = window.getSize();

		// TODO: support multiple anttweakbar windows [10/12/2012 kirschan2]
		//TwSetCurrentWindow( window.getSystemHandle() )
		TwWindowSize( size.x, size.y );
	}

	virtual void onNotify( const EventState &eventState )  {
		TwEventSFML20( &eventState.event );
	}

	virtual void onKeyboard( EventState &eventState )  {
		if( TwEventSFML20( &eventState.event ) ) {
			eventState.accept();
		}
	}

	virtual void onMouse( EventState &eventState )  {
		if( TwEventSFML20( &eventState.event ) ) {
			eventState.accept();
			eventState.setCapture( this, FT_BOTH );
		}
		else {
			eventState.setCapture( this, FT_NONE );
		}
	}

	virtual bool acceptFocus( FocusType focusType )  {
		if( focusType == FT_EXCLUSIVE ) {
			return false;
		}
		return true;
	}

	virtual void loseFocus( FocusType focusType )  {
	}

	virtual void gainFocus( FocusType focusType )  {
	}

	virtual void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime )  {
	}

	void render() {
		TwDraw();
	}
};