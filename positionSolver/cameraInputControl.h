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

	bool getCapture() const {
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
			}
			return true;
		case sf::Event::MouseMoved:
			if( captureMouse ) {
				mouseDelta += sf::Mouse::getPosition() - oldMousePosition;
				oldMousePosition = sf::Mouse::getPosition();
			}			
			return true;
		}
		return false;
	}

private:
	sf::Vector2i mouseDelta, oldMousePosition;
	bool captureMouse;
};

struct CameraInputControl : public MouseCapture {
	std::shared_ptr<Camera> camera;

	void init( const std::shared_ptr<Camera> &camera, const std::shared_ptr<sf::Window> &window ) {
		super::init( window );
		this->camera = camera;
	}

	bool handleEvent( const sf::Event &event ) {
		if( super::handleEvent( event ) ) {
			return true;
		}

		switch( event.type ) {
		case sf::Event::LostFocus:
			setCapture( true );
			break;
		case sf::Event::KeyPressed:
			if( event.key.code == sf::Keyboard::Escape ) {
				setCapture( false );
			}
			return true;
		case sf::Event::MouseButtonReleased:
			if( event.mouseButton.button == sf::Mouse::Left ) {
				setCapture( true );
			}
			return true;
		}
		return false;
	}

	bool update( const float elapsedTime, bool inputProcessed ) {
		if( !inputProcessed && getCapture() ) {
			Eigen::Vector3f relativeMovement = Vector3f::Zero();
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

			relativeMovement *= elapsedTime * 10;

			Eigen::Vector3f newPosition = camera->getPosition() + camera->getViewTransformation().linear().transpose() * relativeMovement;
			camera->setPosition( newPosition );

			sf::Vector2f angleDelta = sf::Vector2f( getMouseDelta() ) * 0.5f;

			camera->yaw( angleDelta.x );
			camera->pitch( angleDelta.y );
		}

		return true;
	}
};