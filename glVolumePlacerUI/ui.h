#pragma once

#include <niven.Engine.Draw2D.h>
#include <niven.Engine.Draw2DRectangle.h>

#include <niven.Engine.Event.EventForwarder.h>

#include <niven.Engine.Event.MouseClickEvent.h>
#include <niven.Engine.Event.MouseMoveEvent.h>
#include <niven.Engine.Event.KeyboardEvent.h>
#include <niven.Engine.Event.MouseWheelEvent.h>
#include <niven.Engine.Event.TextInputEvent.h>
#include <niven.Engine.Event.RenderWindowResizedEvent.h>

#include <niven.Core.Geometry.Rectangle.h>

#include <niven.Render.RenderWindow.h>
#include <niven.Render.RenderContext.h>
#include <niven.Render.RenderSystem.h>

// TODO: remove this using
using namespace niven;

struct UIElement {
	virtual void Draw( Draw2D *draw2D ) {}

	bool MouseMove( const class MouseMoveEvent& event ) {
		bool inArea = IsInArea( event.GetPosition() );
		if( inArea || IsActive() ) {
			return MouseMoveImpl( event, inArea );
		}
		else {
			return false;
		}		
	}

	bool MouseClick( const class MouseClickEvent& event ) {
		if( !IsActive() ) {
			return false;
		}

		return MouseClickImpl( event );
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

	virtual bool IsInArea( const Vector2i &position ) = 0;

	virtual void Unfocus() {}
	virtual void Focus() {}

	virtual bool MouseMoveImpl( const class MouseMoveEvent& event, bool inArea ) {
		return false;
	}

	virtual bool MouseClickImpl( const class MouseClickEvent& event ) {
		return false;
	}
};

struct UIManager : public IEventHandler {
	std::vector<UIElement*> elements;
	UIElement *active;

	Draw2D *draw2D;
	IRenderWindow *renderWindow_;
	
	void Init( Render::IRenderSystem *renderSystem, Render::IRenderContext *renderContext, Render::IRenderWindow *renderWindow ) {
		draw2D = new Draw2D( renderSystem, renderContext );
		draw2D->SetWindowSize( renderWindow->GetWidth(), renderWindow->GetHeight() );
		renderWindow_ = renderWindow;
		active = nullptr;
	}

	bool OnMouseClickImpl( const class MouseClickEvent& event ) 
	{
		if( renderWindow_->IsCursorCaptured() ){
			return false;
		}

		for( int i = 0 ; i < elements.size() ; ++i ) {
			if( elements[i]->MouseClick( event ) ) {
				return true;
			}
		}
		return false;
	}

	bool OnMouseMoveImpl( const class MouseMoveEvent& event ) 
	{
		if( renderWindow_->IsCursorCaptured() ){
			return false;
		}

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

	bool OnRenderWindowResizedImpl( const class RenderWindowResizedEvent& event ) 
	{
		draw2D->SetWindowSize( event.GetWidth(), event.GetHeight() );

		return false;
	}

	void Draw() {
		for( int i = 0 ; i < elements.size() ; ++i ) {
			elements[i]->Draw( draw2D );
		}
	}
};

struct UIButton : public UIElement {
	enum State {
		STATE_INACTIVE,
		STATE_ACTIVE,
		STATE_MOUSE_OVER,
		STATE_CLICKED
	} state;

	Rectangle<int> area;
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
	bool IsInArea( const Vector2i &position ) {
		return visible_ && area.Contains( position );
	}

	bool MouseMoveImpl( const class MouseMoveEvent& event, bool inArea ) {
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

	bool MouseClickImpl( const class MouseClickEvent& event ) {
		bool inArea = IsInArea( event.GetPosition() );
		if( !inArea ) {
			return false;
		}

		if( event.GetButton() == MouseButton::Left ) {			
			if( event.IsPressed() ) { 
				state = STATE_CLICKED;
				if( onAction ) {
					onAction();
				}
			}
			if( event.IsReleased() ) {
				state = inArea ? STATE_MOUSE_OVER : STATE_ACTIVE;
			}
		}
		return true;
	}

	void Draw( Draw2D *draw2D ) {
		if( !visible_ ) {
			return;
		}

		Draw2DRectangle *rect = nullptr;

		switch( state ) {
		case STATE_ACTIVE:
			rect = draw2D->CreateRectangle( Color4f::Constant(0), draw2D->AbsoluteToRelative( area.GetLeft(), area.GetTop() ), Draw2DAnchor::Top_Left, area.GetSize(), Color4f( 0.5, 0.5, 0.5, 1.0 ) );
			break;
		case STATE_MOUSE_OVER:
			rect = draw2D->CreateRectangle( Color4f::Constant(0), draw2D->AbsoluteToRelative( area.GetLeft(), area.GetTop() ), Draw2DAnchor::Top_Left, area.GetSize(), Color4f( 1.0, 1.0, 1.0, 1.0 ) );
			break;
		case STATE_CLICKED:
			rect = draw2D->CreateRectangle( Color4f::Constant(0), draw2D->AbsoluteToRelative( area.GetLeft(), area.GetTop() ), Draw2DAnchor::Top_Left, area.GetSize(), Color4f( 1.0, 0.0, 0.0, 1.0 ) );
			break;
		}

		if( rect ) {
			draw2D->Draw( rect );
			draw2D->Release( rect );
		}
	}
};