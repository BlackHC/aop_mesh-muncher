#include "widgets.h"

#include <gl/glew.h>
#include <unsupported/Eigen/OpenGLSupport>
#include <debugRender.h>

TransformChain::TransformChain() : localTransform( Eigen::Affine3f::Identity() ), globalTransform( Eigen::Affine3f::Identity() ) {}

void TransformChain::update( TransformChain *parent ) {
	if( parent ) {
		globalTransform = parent->globalTransform * localTransform;
	}
	else {
		globalTransform = localTransform;
	}
}

void TransformChain::setOffset( const Eigen::Vector2f &offset ) {
	localTransform = Eigen::Translation3f( Eigen::Vector3f( offset[0], offset[1], 0.0f ) );
}

Eigen::Vector2f TransformChain::getOffset() const {
	return localTransform.translation().head<2>();
}

Eigen::Vector2f TransformChain::pointToScreen( const Eigen::Vector2f &point ) const {
	return ( globalTransform * Eigen::Vector3f( point[0], point[1], 0.0f ) ).head<2>();
}

Eigen::Vector2f TransformChain::vectorToScreen( const Eigen::Vector2f &point ) const {
	return ( globalTransform.linear() * Eigen::Vector3f( point[0], point[1], 0.0f ) ).head<2>();
}

Eigen::Vector2f TransformChain::screenToPoint( const Eigen::Vector2f &point ) const {
	return ( globalTransform.inverse() * Eigen::Vector3f( point[0], point[1], 0.0f ) ).head<2>();
}

Eigen::Vector2f TransformChain::screenToVector( const Eigen::Vector2f &point ) const {
	return ( globalTransform.inverse().linear() * Eigen::Vector3f( point[0], point[1], 0.0f ) ).head<2>();
}

void WidgetBase::onRender() {
	glPushMatrix();
	Eigen::glLoadMatrix( transformChain.globalTransform );
	doRender();
	glPopMatrix();
}

void WidgetBase::onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
	transformChain.update( &parent->transformChain );

	doUpdate( eventSystem, frameDuration, elapsedTime );
}

void ButtonWidget::setState( State newState ) {
	if( state == newState ) {
		return;
	}

	if( newState == STATE_CLICKED ) {
		onAction();
	}

	if( newState == STATE_INACTIVE ) {
		onMouseLeave();
	}
	else if( state == STATE_INACTIVE ) {
		onMouseEnter();
	}

	state = newState;
}

void ButtonWidget::onMouse( EventState &eventState ) {
	bool hasMouse = eventState.getCapture( this ) == FT_MOUSE;
	switch( eventState.event.type ) {
	case sf::Event::MouseMoved:
		if( isInArea( Eigen::Vector2f( eventState.event.mouseMove.x, eventState.event.mouseMove.y ) ) ) {
			eventState.setCapture( this, FT_MOUSE );

			if( !hasMouse && state != STATE_CLICKED ) {
				setState( STATE_HOVER );
			}

			eventState.accept();
		}
		else if( !sf::Mouse::isButtonPressed( sf::Mouse::Left ) ){
			eventState.setCapture( this, FT_NONE );
			setState( STATE_INACTIVE );
		}

		break;
	case sf::Event::MouseButtonPressed:
		if( hasMouse ) {
			setState( STATE_CLICKED );
			eventState.accept();
		}
		break;
	case sf::Event::MouseButtonReleased:
		if( hasMouse ) {
			eventState.accept();
		}
		if( state == STATE_CLICKED ) {
			if( isInArea( Eigen::Vector2f( eventState.event.mouseButton.x, eventState.event.mouseButton.y ) ) ) {
				setState( STATE_HOVER );
			}
			else {
				setState( STATE_INACTIVE );
			}
		}
		break;
	}
}

void ButtonWidget::doRender() {
	// render the background
	switch( state ) {
	case STATE_HOVER:
		glColor4f( 0.2f, 0.2f, 0.9f, 0.8f );
		break;
	case STATE_CLICKED:
		glColor4f( 0.7f, 0.7f, 0.7f, 0.8f );
		break;
	default:
		glColor4f( 0.2f, 0.2f, 0.2f, 0.8f );
		break;
	}
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	DebugRender::drawQuad( Eigen::Vector2f::Zero(), size, false );
	glDisable( GL_BLEND );

	// render the border
	switch( state ) {
	case STATE_HOVER:
		DebugRender::setColor( Eigen::Vector3f( 1.0f, 1.0f, 1.0f ) );
		break;
	case STATE_CLICKED:
		DebugRender::setColor( Eigen::Vector3f( 1.0f, 0.0f, 0.0f ) );
		break;
	default:
		DebugRender::setColor( Eigen::Vector3f( 0.5f, 0.5f, 0.5f ) );
		break;
	}
	DebugRender::drawQuad( Eigen::Vector2f::Zero(), size );

	doRenderInside();
}

void ProgressBarWidget::doRender() {
	// render the progress bar
	// we blend between dark/bluish and light green
	const Eigen::Vector3f zero( 0.0f, 0.5f, 0.3f );
	const Eigen::Vector3f one( 0.3f, 1.0f, 0.3f );
	DebugRender::setColor( one * percentage + (1.0 - percentage) * zero );
	DebugRender::drawQuad( Eigen::Vector2f::Zero(), size.cwiseProduct( Eigen::Vector2f( percentage, 1.0f ) ), false );

	// render the border
	DebugRender::setColor( Eigen::Vector3f::Constant( 0.5f ) );
	DebugRender::drawQuad( Eigen::Vector2f::Zero(), size );
}
