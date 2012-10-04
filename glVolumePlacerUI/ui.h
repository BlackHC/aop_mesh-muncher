#pragma once

#include "debugRender.h"
#include "eventHandling.h"
#include <Eigen/Eigen>

// TODO: rework callback parameters to be library agnostic
struct UIElement {
	virtual void Draw() {}

	bool MouseMove( const sf::Event::MouseMoveEvent &event ) {
		bool inArea = IsInArea( Eigen::Vector2i( event.x, event.y ) );
		if( inArea || IsActive() ) {
			return MouseMoveImpl( event, inArea );
		}
		else {
			return false;
		}		
	}

	bool MouseClick( const sf::Event::MouseButtonEvent &event, bool pressed ) {
		if( !IsActive() ) {
			return false;
		}

		return MouseClickImpl( event, pressed );
	}

	bool IsActive() const {
		return active_;
	}

	void SetActive( bool active ) {
		if( active_ != active ) {
			active_ = active;		
			if( active_ ) {
				Focus();
			}
			else {
				Unfocus();
			}
		}		
	}

private:
	bool active_;

	virtual bool IsInArea( const Eigen::Vector2i &position ) = 0;

	virtual void Unfocus() {}
	virtual void Focus() {}

	virtual bool MouseMoveImpl( const sf::Event::MouseMoveEvent &event, bool inArea ) {
		return false;
	}

	virtual bool MouseClickImpl( const sf::Event::MouseButtonEvent &event, bool pressed ) {
		return false;
	}
};

struct UIManager : public EventHandler {
	std::vector<UIElement*> elements;
	UIElement *active;

	void Init() {
		active = nullptr;
	}

	bool OnMouseClickImpl( const sf::Event::MouseButtonEvent &event, bool pressed ) 
	{
		/*if( renderWindow_->IsCursorCaptured() ){
			return false;
		}*/
		
		for( int i = 0 ; i < elements.size() ; ++i ) {
			if( elements[i]->MouseClick( event, pressed ) ) {
				return true;
			}
		}
		return false;
	}

	bool OnMouseMoveImpl( const sf::Event::MouseMoveEvent & event ) 
	{
		/*if( renderWindow_->IsCursorCaptured() ){
			return false;
		}*/

		for( int i = 0 ; i < elements.size() ; ++i ) {
			if( elements[i]->MouseMove( event ) ) {
				if( elements[i]->IsActive() ) {
					// update the active element pointer if necessary and steal the focus from the previous active element
					if( active != elements[i] && active ) {
						active->SetActive( false );
					}
					active = elements[i];
				}
				return true;
			}
		}
		return false;
	}

	void Draw() {
		for( int i = 0 ; i < elements.size() ; ++i ) {
			elements[i]->Draw();
		}
	}

	virtual bool handleEvent( const sf::Event &event ) 
	{
		switch( event.type ) {
		case sf::Event::MouseMoved:
			return OnMouseMoveImpl( event.mouseMove );
		case sf::Event::MouseButtonReleased:
			return OnMouseClickImpl( event.mouseButton, false );
		case sf::Event::MouseButtonPressed:
			return OnMouseClickImpl( event.mouseButton, true );
		}
		return false;
	}
};

struct UIButton : public UIElement {
	enum State {
		STATE_INACTIVE,
		STATE_ACTIVE,
		STATE_MOUSE_OVER,
		STATE_CLICKED
	} state;

	Eigen::AlignedBox2i area;
	std::function<void ()> onAction, onFocus, onUnfocus;	

	bool IsVisible() const {
		return visible_;
	}

	void SetVisible( bool visible ) {
		if( !visible ) {
			SetActive( false );
			state = STATE_INACTIVE;
		}
		visible_ = visible;
	}

protected:
	bool visible_;

private:
	bool IsInArea( const Eigen::Vector2i &position ) {
		return visible_ && area.contains( position );
	}

	bool MouseMoveImpl( const sf::Event::MouseMoveEvent& event, bool inArea ) {
		switch( state ) {
		case STATE_CLICKED:
			return true;
		case STATE_INACTIVE:
		case STATE_ACTIVE:
		case STATE_MOUSE_OVER:
			SetActive( true );
			if( inArea ) {
				state = STATE_MOUSE_OVER;
			}
			else {
				SetActive( false );
			}
			break;
		}

		return inArea;
	}

	void Focus() {
		state = STATE_ACTIVE;
		if( onFocus )
			onFocus();
	}

	void Unfocus() {
		state = STATE_INACTIVE;
		if( onUnfocus )
			onUnfocus();
	}

	bool MouseClickImpl( const sf::Event::MouseButtonEvent& event, bool pressed ) {
		bool inArea = IsInArea( Eigen::Vector2i( event.x, event.y ) );
		if( !inArea ) {
			return false;
		}

		if( event.button == sf::Mouse::Left ) {
			if( pressed ) { 
				state = STATE_CLICKED;
				if( onAction ) {
					onAction();
				}
			}
			else {
				state = inArea ? STATE_MOUSE_OVER : STATE_ACTIVE;
			}
		}
		return true;
	}

	void Draw() {
		if( !visible_ /*|| state == STATE_INACTIVE*/ ) {
			return;
		}

		DebugRender::ImmediateCalls render;
		render.begin();
		switch( state ) {
		case STATE_ACTIVE:
			render.setColor( Vector3f( 0.5, 0.5, 0.5 ) );
			break;
		case STATE_MOUSE_OVER:
			render.setColor( Vector3f( 1.0, 1.0, 1.0 ) );
			break;
		case STATE_CLICKED:
			render.setColor( Vector3f( 1.0, 0.0, 0.0 ) );
			break;
		default:
			render.setColor( Vector3f( 1.0, 0.0, 1.0 ) );
			break;
		}

		render.drawAABB( Vector3f( area.min().x(), area.min().y(), 0.0 ), Vector3f( area.max().x(), area.max().y(), 0.0 ) );
		render.end();
	}
};