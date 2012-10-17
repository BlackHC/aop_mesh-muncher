#pragma once

#include <Eigen/Eigen>
#include <eventHandling.h>

// The widgets use a 0..1x0..1 relative coordinate system with 0,0 being in the top left corner

struct TransformChain {
	Eigen::Affine3f localTransform, globalTransform;

	TransformChain();

	void update( TransformChain *parent = nullptr );
	void setOffset( const Eigen::Vector2f &offset );
	Eigen::Vector2f getOffset() const;

	void setScale( float scale );
	float getScale() const;

	// screen: absolute screen/window coordinates
	// local: 0..1
	
	Eigen::Vector2f pointToScreen( const Eigen::Vector2f &point ) const;
	
	Eigen::Vector2f vectorToScreen( const Eigen::Vector2f &point ) const;

	Eigen::Vector2f screenToPoint( const Eigen::Vector2f &point ) const;
	Eigen::Vector2f screenToVector( const Eigen::Vector2f &point ) const;
};

struct ITransformChain : virtual EventHandler {
	TransformChain transformChain;
};

struct IWidget : virtual ITransformChain, virtual EventHandler::WithParentDecl< ITransformChain > {
	typedef std::shared_ptr< IWidget > SPtr;

	virtual void onRender() = 0;
};

// TODO: this is a huge cluster fuck (together with WidgetRoot) and way too dependent on implementation details.. [10/4/2012 kirschan2]
struct WidgetBase : TemplateNullEventHandler< ITransformChain, IWidget > {
	void onRender();
	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime );

private:
	virtual void doRender() = 0;
	virtual void doUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) = 0;
};

struct WidgetContainer : TemplateEventDispatcher< IWidget, EventHandler::WithSimpleParentImpl< ITransformChain, IWidget > > {
	void onRender() {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			eventHandler->get()->onRender();
		}
	}

	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
		transformChain.update( &parent->transformChain );

		Base::onUpdate( eventSystem, frameDuration, elapsedTime );
	}
};

struct WidgetRoot : TemplateEventDispatcher< IWidget, EventHandler::WithSimpleParentImpl<EventHandler, ITransformChain> > {
	void onRender() {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			eventHandler->get()->onRender();
		}
	}

	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
		transformChain.update();

		Base::onUpdate( eventSystem, frameDuration, elapsedTime );
	}
};

struct ProgressBarWidget : WidgetBase {
	Eigen::Vector2f size;

	float percentage;
	
	ProgressBarWidget( float percentage, const Eigen::Vector2f &offset, const Eigen::Vector2f &size ) 
		: percentage( percentage )
		, size( size )
	{
		transformChain.setOffset( offset );
	}

	void doRender();
	void doUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {}
};

struct ButtonWidget : WidgetBase {
	enum State {
		STATE_INACTIVE,
		STATE_HOVER,
		STATE_CLICKED
	} state;

	Eigen::Vector2f size;

	ButtonWidget( const Eigen::Vector2f &offset, const Eigen::Vector2f &size ) 
		: state()
		, size( size )
	{
		transformChain.setOffset( offset );
	}

	bool isInArea( const Eigen::Vector2f &globalPosition ) {
		const Eigen::Vector2f localPosition = transformChain.screenToPoint( globalPosition );
		if( localPosition[0] >= 0 && localPosition[1] >= 0 &&
			localPosition[0] <= size[0] && localPosition[1] <= size[1]
		) {
			return true;
		}
		return false;
	}

	void setState( State newState );

	void onMouse( EventState &eventState );

	void doUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
	}

	bool acceptFocus( FocusType focusType ) {
		if( focusType == FT_MOUSE ) {
			return true;
		}
		return false;
	}

	void loseFocus( FocusType focusType ) {
		setState( STATE_INACTIVE );
	}

private:
	void doRender();

	virtual void onAction() = 0;
	virtual void doRenderInside() = 0;
	virtual void onMouseEnter() = 0;
	virtual void onMouseLeave() = 0;
};

struct DummyButtonWidget : ButtonWidget {
	void onAction() {}
	void onMouseEnter() {}
	void onMouseLeave() {}
	void doRenderInside() {}
};

