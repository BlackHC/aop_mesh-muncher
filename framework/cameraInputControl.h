#pragma once

#include "camera.h"
#include "eventHandling.h"

struct MouseCapture : public EventHandler  {
	typedef MouseCapture super;

	std::shared_ptr<sf::Window> window;

	MouseCapture() : captureMouse( false ) {}

	void init( const std::shared_ptr<sf::Window> &window ) {
		this->window = window;
	}

	bool hasCapture() const {
		return captureMouse;
	}

	sf::Vector2i getMouseDelta() {
		sf::Vector2i temp = mouseDelta;
		mouseDelta = sf::Vector2i();
		return temp;
	}

	void setCapture( bool active ) {
		if( captureMouse == active ) {
			return;
		}

		mouseDelta = sf::Vector2i();

		if( active ) {
			oldMousePosition = sf::Mouse::getPosition();
		}
		// TODO: add support for ClipCursor?
		window->setMouseCursorVisible( !active );

		captureMouse = active;
	}

	bool handleEvent( const sf::Event &event ) {
		switch( event.type ) {
		case sf::Event::MouseLeft:
			if( captureMouse ) {
				sf::Mouse::setPosition( sf::Vector2i( window->getSize() / 2u ), *window );	
				oldMousePosition = sf::Mouse::getPosition();
				return true;
			}
			break;
		case sf::Event::MouseMoved:
			if( captureMouse ) {
				mouseDelta += sf::Mouse::getPosition() - oldMousePosition;
				oldMousePosition = sf::Mouse::getPosition();
				return true;
			}			
			break;
		}
		return false;
	}

private:
	sf::Vector2i mouseDelta, oldMousePosition;
	bool captureMouse;
};

struct CameraInputControl : public MouseCapture {
	std::shared_ptr<Camera> camera;
	float moveSpeed;

	void init( const std::shared_ptr<Camera> &camera, const std::shared_ptr<sf::Window> &window ) {
		super::init( window );
		this->camera = camera;
		this->moveSpeed = 10.0f;
	}

	bool handleEvent( const sf::Event &event ) {
		if( super::handleEvent( event ) ) {
			return true;
		}

		switch( event.type ) {
		case sf::Event::LostFocus:
			setCapture( false );
			return false;
		case sf::Event::KeyPressed:
			if( event.key.code == sf::Keyboard::Escape ) {
				setCapture( false );
				return true;
			}
			break;
		case sf::Event::MouseButtonPressed:
			if( event.mouseButton.button == sf::Mouse::Left ) {
				setCapture( true );
			}
			return true;
		case sf::Event::MouseWheelMoved:
			if( hasCapture() ) {
				moveSpeed *= std::pow( 1.5f, (float) event.mouseWheel.delta );
				return true;
			}
		}
		return false;
	}

	bool update( const float elapsedTime, bool inputProcessed ) {
		if( !inputProcessed && hasCapture() ) {
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

			relativeMovement *= elapsedTime * moveSpeed;
			if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
				relativeMovement *= 4;
			}

			Eigen::Vector3f newPosition = camera->getPosition() + camera->getViewTransformation().linear().transpose() * relativeMovement;
			camera->setPosition( newPosition );

			sf::Vector2f angleDelta = sf::Vector2f( getMouseDelta() ) * 0.5f;

			camera->yaw( angleDelta.x );
			camera->pitch( angleDelta.y );
		}

		return true;
	}
};