#pragma once

#include "camera.h"
#include "eventHandling.h"


struct CameraInputControl : EventHandler::WithDefaultParentImpl {
	std::shared_ptr<Camera> camera;
	float moveSpeed;

	void init( const std::shared_ptr<Camera> &camera ) {
		this->camera = camera;
		this->moveSpeed = 10.0f;
	}
	
	virtual void onNotify( const EventState &eventState )  {
		switch( eventState.event.type ) {
		case sf::Event::LostFocus:
			eventState.setCapture( this, FT_NONE );
			break;
		}
	}

	virtual void onKeyboard( EventState &eventState )  {
		switch( eventState.event.type ) {
		case sf::Event::KeyPressed:
			if( eventState.event.key.code == sf::Keyboard::Escape ) {
				eventState.setCapture( this, FT_NONE );
				eventState.accept();
			}
			break;
		}
	}

	virtual void onMouse( EventState &eventState )  {
		switch( eventState.event.type ) {
		case sf::Event::MouseButtonPressed:
			if( eventState.event.mouseButton.button == sf::Mouse::Left ) {
				eventState.setCapture( this, FT_EXCLUSIVE );
				eventState.accept();
			}
			break;
		case sf::Event::MouseButtonReleased:
			if( eventState.event.mouseButton.button == sf::Mouse::Left ) {
				eventState.setCapture( nullptr, FT_EXCLUSIVE );
				eventState.accept();
			}
			break;
		case sf::Event::MouseWheelMoved:
			if( eventState.isExclusive( this ) ) {
				moveSpeed *= std::pow( 1.5f, (float) eventState.event.mouseWheel.delta );
				eventState.accept();
			}
			break;
		}
	}

	virtual bool acceptFocus( FocusType focusType )  {
		return true;
	}

	virtual void loseFocus( FocusType focusType )  {
	}

	virtual void gainFocus( FocusType focusType )  {
	}

	virtual void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime )  {
		if( eventSystem.getCapture( this ) & FT_EXCLUSIVE ) {
			Eigen::Vector3f relativeMovement = Eigen::Vector3f::Zero();
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::W ) ) {
				relativeMovement.z() -= 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::S ) ) {
				relativeMovement.z() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::A ) ) {
				relativeMovement.x() -= 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::D ) ) {
				relativeMovement.x() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::Space ) ) {
				relativeMovement.y() += 1;
			}
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::LControl ) ) {
				relativeMovement.y() -= 1;
			}

			if( !relativeMovement.isZero() ) {
				relativeMovement.normalize();
			}

			relativeMovement *= frameDuration * moveSpeed;
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
				relativeMovement *= 4;
			}

			Eigen::Vector3f newPosition = camera->getPosition() + camera->getViewTransformation().linear().transpose() * relativeMovement;
			camera->setPosition( newPosition );

			sf::Vector2f angleDelta = sf::Vector2f( getEventSystem()->exclusiveMode.popMouseDelta() ) * 0.5f;

			camera->yaw( angleDelta.x );
			camera->pitch( angleDelta.y );
		}
	}

};