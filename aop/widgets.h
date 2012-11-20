#pragma once

#include <Eigen/Eigen>
#include <eventHandling.h>
#include <mathUtility.h>

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

	Eigen::Vector2f pointToParent( const Eigen::Vector2f &point ) const;
	Eigen::Vector2f vectorToParent( const Eigen::Vector2f &point ) const;

	Eigen::Vector2f parentToPoint( const Eigen::Vector2f &point ) const;
	Eigen::Vector2f parentToVector( const Eigen::Vector2f &point ) const;
};

struct ITransformChain : virtual EventHandler {
	TransformChain transformChain;
};

struct IWidget : virtual ITransformChain, virtual EventHandler::WithParentDecl< ITransformChain > {
	typedef std::shared_ptr< IWidget > SPtr;

	virtual void onRender() = 0;

	// returns the area in the parent space---not screen!
	Eigen::AlignedBox2f getArea() {
		const auto localArea = getLocalArea();

		Eigen::AlignedBox2f area;
		area.extend( transformChain.pointToParent( localArea.min() ) );
		area.extend( transformChain.pointToParent( localArea.max() ) );
		return area;
	}

	// get local area
	virtual Eigen::AlignedBox2f getLocalArea() = 0;

	// return true if anything has changed
	// update layout should be called for every (visible) child ) (no early out on the first true!)
	//virtual bool updateLayout() = 0;
};

// TODO: this is a huge cluster fuck (together with WidgetRoot) and way too dependent on implementation details.. [10/4/2012 kirschan2]
struct WidgetBase : TemplateNullEventHandler< ITransformChain, IWidget > {
	void onRender();
	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime );

	//Eigen::AlignedBox2f getArea() = 0;

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

	Eigen::AlignedBox2f getLocalChildArea() {
		Eigen::AlignedBox2f area;
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			area.extend( eventHandler->get()->getArea() );
		}
		return area;
	}

	Eigen::AlignedBox2f getLocalArea() {
		return getLocalChildArea();
	}

	/*bool updateLayout() {
		bool needsUpdate = false;
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			needsUpdate = needsUpdate || eventHandler->updateLayout();
		}
		return needsUpdate;
	}*/
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

	Eigen::AlignedBox2f getLocalArea() {
		return Eigen::AlignedBox2f( Eigen::Vector2f::Zero(), size );
	}
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

	Eigen::AlignedBox2f getLocalArea() {
		return Eigen::AlignedBox2f( Eigen::Vector2f::Zero(), size );
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

struct ScrollableContainer : WidgetContainer {
	Eigen::Vector2f size;

	Eigen::AlignedBox2f scrollArea;
	
	float scrollStep;

	bool verticalScrollByDefault;
	
	ScrollableContainer( const Eigen::Vector2f &size = Eigen::Vector2f::Zero() ) 
		: scrollStep() 
		, size( size )
		, scrollArea( Eigen::Vector2f::Zero() )
		, verticalScrollByDefault( true )
	{}

	void onMouse( EventState &eventState ) {
		if( eventState.event.type == sf::Event::MouseWheelMoved ) {
			//log( boost::format( "mouse wheel moved %i" ) % eventState.event.mouseWheel.delta );

			// we use the offset to change the origin from -(length - scrollArea.max) to -scrollArea.min
			auto offset = transformChain.getOffset();
			if( verticalScrollByDefault ) {
				offset[1] = clamp( offset[1] + eventState.event.mouseWheel.delta * scrollStep, size[1] - scrollArea.max()[1], -scrollArea.min()[1] );
			}
			else {
				offset[0] = clamp( offset[0] + eventState.event.mouseWheel.delta * scrollStep, size[0] - scrollArea.max()[0], -scrollArea.min()[0] );
			}
			transformChain.setOffset( offset );

			eventState.accept();
		}
		else {
			WidgetContainer::onMouse( eventState );
		}
	}

	void updateScrollArea() {
		scrollArea = getLocalChildArea();
	}

	Eigen::AlignedBox2f getLocalArea() {
		return Eigen::AlignedBox2f( Eigen::Vector2f::Zero(), size );
	}
};

// we set a scissor rectangle here to the area of the children
struct ClippedContainer : WidgetContainer  {
	Eigen::AlignedBox2f localArea;
	bool visible;

	ClippedContainer()
		: localArea()
		, visible( true )
	{
	}

	void onRender();

	void onKeyboard( EventState &eventState ) {
		if( visible ) {
			WidgetContainer::onKeyboard( eventState );
		}
	}

	void onMouse( EventState &eventState ) {
		if( visible ) {
			WidgetContainer::onMouse( eventState );
		}
	}

	bool acceptFocus( FocusType focusType ) {
		if( !visible ) {
			return false;
		}
		return WidgetContainer::acceptFocus( focusType );
	}

	void gainFocus( FocusType focusType )  {
		visible = true;
		WidgetContainer::gainFocus( focusType );
	}

	virtual Eigen::AlignedBox2f getLocalArea() {
		return localArea;
	}

	void updateLocalArea() {
		localArea = getLocalChildArea();
	}
};