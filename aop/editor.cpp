#include "editor.h"

using namespace Eigen;

void Editor::init() {
	modes.addEventHandler( make_nonallocated_shared( selecting ) );
	modes.addEventHandler( make_nonallocated_shared( placing ) );
	modes.addEventHandler( make_nonallocated_shared( moving ) );
	modes.addEventHandler( make_nonallocated_shared( rotating ) );
	modes.addEventHandler( make_nonallocated_shared( resizing ) );

	addEventHandler( make_nonallocated_shared( modes ) );

	addEventHandler( std::make_shared<KeyAction>( "enter selection mode", sf::Keyboard::F6, [&] () {
		selectMode( &selecting );
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter placement mode", sf::Keyboard::F7, [&] () {
		if( transformer ) {
			selectMode( &placing );
		}
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter movement mode", sf::Keyboard::F8, [&] () {
		if( transformer ) {
			selectMode( &moving );
		}
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter rotation mode", sf::Keyboard::F9, [&] () {
		if( transformer ) {
			selectMode( &rotating );
		}
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter resize mode", sf::Keyboard::F10, [&] () {
		if( transformer && transformer->canResize() ) {
			selectMode( &resizing );
		}
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter free-look mode", sf::Keyboard::F5, [&] () {
		selectMode( nullptr );
	} ) );
}

void Editor::render() {
	DebugRender::begin();
	DebugRender::setColor( Eigen::Vector3f::UnitX() );
	if( transformer ) {
		DebugRender::setTransformation( transformer->getTransformation() );
		DebugRender::drawBox( transformer->getSize() );
	}

	// render obbs
	DebugRender::setColor( Eigen::Vector3f::Constant( 1.0 ) );
	for( auto obb = obbs.begin() ; obb != obbs.end() ; ++obb ) {
		DebugRender::setTransformation( obb->transformation );
		DebugRender::drawBox( obb->size );	
	}		
	DebugRender::end();

	if( modes.target ) {
		modes.target->render();
	}
}

void Editor::selectMode( Mode *mode ) {
	modes.setTarget( mode );
	eventSystem->setCapture( mode,  FT_KEYBOARD );
}

void Editor::convertScreenToHomogeneous( int x, int y, float &xh, float &yh ) {
	// TODO: refactor [9/30/2012 kirschan2]
	const sf::Vector2i size( eventSystem->exclusiveMode.window->getSize() );
	xh = float( x ) / size.x * 2 - 1;
	yh = -(float( y ) / size.y * 2 - 1);
}

Eigen::Vector3f Editor::getScaledRelativeViewMovement( const Eigen::Vector3f &relativeMovement ) {
	Eigen::Vector3f u, v, w;
	Eigen::unprojectAxes( view->viewerContext.worldViewerPosition, view->viewerContext.projectionView, u, v, w );

	const float scale = w.dot( transformer->getTransformation().translation() - view->viewerContext.worldViewerPosition ) / w.squaredNorm();

	return
		u * scale * relativeMovement.x() +
		v * scale * relativeMovement.y() +
		w * scale * relativeMovement.z()
		;
}

void Editor::TransformMode::storeState() {
	storedTransformation = editor->transformer->getTransformation();
}

void Editor::TransformMode::restoreState() {
	editor->transformer->setTransformation( storedTransformation );
}

void Editor::TransformMode::onMouse( EventState &eventState ) {
	switch( eventState.event.type ) {
	case sf::Event::MouseWheelMoved:
		transformSpeed *= std::pow( 1.5f, (float) eventState.event.mouseWheel.delta );
		eventState.accept();
		break;
	case sf::Event::MouseButtonPressed:
		if( eventState.event.mouseButton.button == sf::Mouse::Left ) {
			startDragging();
			eventState.accept();
		}
		break;
	case sf::Event::MouseButtonReleased:
		if( eventState.event.mouseButton.button == sf::Mouse::Left ) {
			stopDragging( true );
			eventState.accept();
		}
		break;
	}
	if( dragging ) {
		eventState.accept();
	}
}

void Editor::TransformMode::onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
	if( !selected ) {
		return;
	}

	if( !dragging ) {
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

		relativeMovement *= frameDuration * transformSpeed;
		if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
			relativeMovement *= 4;
		}

		transform( relativeMovement );
	}
	else {
		const sf::Vector2i draggedDelta = popMouseDelta();

		// TODO: get camera viewport size
		Eigen::Vector3f relativeMovement( draggedDelta.x, -draggedDelta.y, 0.0 );

		if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
			relativeMovement *= 4;
		}
		relativeMovement *= 0.01;

		transform( relativeMovement );
	}
}

void Editor::TransformMode::onKeyboard( EventState &eventState ) {
	switch( eventState.event.type ) {
	case sf::Event::KeyPressed:
		if( eventState.event.key.code == sf::Keyboard::Escape && dragging ) {
			stopDragging( false );
			eventState.accept();
		}
		break;
	}
}

void Editor::TransformMode::onNotify( const EventState &eventState ) {
	if( eventState.event.type == sf::Event::LostFocus ) {
		stopDragging( false );
	}
}

void Editor::Selecting::onMouse( EventState &eventState ) {
	if( eventState.event.type == sf::Event::MouseButtonPressed && eventState.event.mouseButton.button == sf::Mouse::Left ) {
		editor->transformer.reset();

		float xh, yh;
		editor->convertScreenToHomogeneous( eventState.event.mouseButton.x, eventState.event.mouseButton.y, xh, yh );

		// duplicate from resizing [9/30/2012 kirschan2]
		const Eigen::Vector4f nearPlanePoint( xh, yh, -1.0, 1.0 );
		const Eigen::Vector3f direction = ((editor->view->viewerContext.projectionView.inverse() * nearPlanePoint).hnormalized() - editor->view->viewerContext.worldViewerPosition).normalized();

		float bestT;
		int bestOBB = -1;
		for( int obbIndex = 0 ; obbIndex < editor->volumes->getCount() ; obbIndex++ ) {
			const OBB *obb = editor->volumes->get( obbIndex );
			float t;

			if( intersectRayWithOBB( *obb, editor->view->viewerContext.worldViewerPosition, direction, nullptr, &t ) ) {
				if( bestOBB == -1 || bestT > t ) {
					bestT = t;
					bestOBB = obbIndex;
				}
			}
		}

		// NOTE: this all only works if direction is normalized, so we can compare hitDistance with bestT! [9/30/2012 kirschan2]
		SGSInterface::SelectionResult result;
		if( editor->world->selectFromView( *editor->view, xh, yh, &result ) && result.objectIndex != SGSInterface::SelectionResult::SELECTION_INDEX_TERRAIN ) {
			if( bestOBB == -1 || result.hitDistance < bestT) {
				bestOBB = -1;
				editor->transformer = std::make_shared< SGSInstanceTransformer >( editor, result.objectIndex );
			}
		}

		if( bestOBB != -1 ) {
			editor->transformer = std::make_shared< OBBTransformer >( editor, bestOBB );
		}
	}
	eventState.accept();
}

void Editor::Placing::onMouse( EventState &eventState ) {
	if( eventState.event.type == sf::Event::MouseButtonPressed && eventState.event.mouseButton.button == sf::Mouse::Left ) {
		float xh, yh;
		editor->convertScreenToHomogeneous( eventState.event.mouseButton.x, eventState.event.mouseButton.y, xh, yh );

		SGSInterface::SelectionResult result;
		if( editor->world->selectFromView( *editor->view, xh, yh, &result ) ) {
			const auto transformation = editor->transformer->getTransformation();
			editor->transformer->setTransformation(
				Eigen::Translation3f( Eigen::map( result.hitPosition) ) * transformation.linear()
				);
		}
	}
	eventState.accept();
}

void Editor::Moving::transform( const Eigen::Vector3f &relativeMovement ) {
	const Eigen::Translation3f translation( editor->getScaledRelativeViewMovement( relativeMovement ) );

	editor->transformer->setTransformation( translation * editor->transformer->getTransformation() );
}

void Editor::Rotating::transform( const Eigen::Vector3f &relativeMovement ) {
	const auto rotation =
		Eigen::AngleAxisf( relativeMovement.z(), editor->view->viewAxes.col(2) ) *
		Eigen::AngleAxisf( relativeMovement.y(), editor->view->viewAxes.col(0) ) *
		Eigen::AngleAxisf( relativeMovement.x(), editor->view->viewAxes.col(1) )
		;

	const Vector3f translation = editor->transformer->getTransformation().translation();

	editor->transformer->setTransformation(
		Eigen::Translation3f( translation ) *
		rotation *
		Eigen::Translation3f( -translation ) *
		editor->transformer->getTransformation()
		);
}

void Editor::Resizing::storeState() {
	storedOBB = editor->transformer->getOBB();
}

void Editor::Resizing::restoreState() {
	editor->transformer->setOBB( storedOBB );
}

bool Editor::Resizing::setCornerMasks( int x, int y ) {
	// TODO: get window/viewport size [9/28/2012 kirschan2]
	// camera property?
	float xh, yh;
	editor->convertScreenToHomogeneous( x, y, xh, yh );

	const Eigen::Vector4f nearPlanePoint( xh, yh, -1.0, 1.0 );
	const Eigen::Vector3f direction = (editor->view->viewerContext.projectionView.inverse() * nearPlanePoint).hnormalized() - editor->view->viewerContext.worldViewerPosition;

	const OBB objectOBB = editor->transformer->getOBB();

	Eigen::Vector3f boxHitPoint;
	//Eigen::Vector3f hitPoint;
	if( intersectRayWithOBB( objectOBB, editor->view->viewerContext.worldViewerPosition, direction, &hitPoint ) ) {
		boxHitPoint = objectOBB.transformation.inverse() * hitPoint;
	}
	else {
		const auto objectDirection = objectOBB.transformation.translation() - editor->view->viewerContext.worldViewerPosition;
		hitPoint = editor->view->viewerContext.worldViewerPosition +
			direction.normalized() / direction.normalized().dot( objectDirection ) * objectDirection.squaredNorm();
		hitPoint = objectOBB.transformation.inverse() * hitPoint;
		boxHitPoint = nearestPointOnAABoxToPoint( -objectOBB.size / 2, objectOBB.size / 2, hitPoint );
		hitPoint = objectOBB.transformation * boxHitPoint;
	}

	// determine the nearest plane, edge whatever
	const Eigen::Vector3f p = boxHitPoint.cwiseQuotient( objectOBB.size / 2.0 );

	int maskFlag = 0;
	for( int i = 0 ; i < 3 ; i++ ) {
		// 20% border
		if( fabs( p[i] ) > 0.6 ) {
			maskFlag |= 1<<i;
		}
	}

	mask.setZero();
	invertedMask.setZero();

	for( int i = 0 ; i < 3 ; i++ ) {
		float value = (p[i] > 0.0) * 2 - 1;
		if( maskFlag & (1<<i) ) {
			mask[i] = value;
		}
		else {
			invertedMask[i] = value;
		}
	}

	return true;
}

void Editor::Resizing::onMouse( EventState &eventState ) {
	switch( eventState.event.type ) {
	case sf::Event::MouseWheelMoved:
		transformSpeed *= std::pow( 1.5f, (float) eventState.event.mouseWheel.delta );
		break;
	case sf::Event::MouseButtonPressed:
		if( eventState.event.mouseButton.button == sf::Mouse::Button::Left && setCornerMasks( eventState.event.mouseButton.x, eventState.event.mouseButton.y ) ) {
			startDragging();
			eventState.accept();
		}
		break;
	case sf::Event::MouseButtonReleased:
		if( eventState.event.mouseButton.button == sf::Mouse::Button::Left ) {
			stopDragging( true );
		}
		break;
	}

	eventState.accept();
}

void Editor::Resizing::onKeyboard( EventState &eventState ) {
	if( eventState.event.type == sf::Event::KeyPressed && eventState.event.key.code == sf::Keyboard::Escape && dragging ) {
		stopDragging( false );
	}
	// always consume keys we handle in update
	switch( eventState.event.key.code ) {
	case sf::Keyboard::Escape:
	case sf::Keyboard::LShift:
	case sf::Keyboard::LControl:
		eventState.accept();
		break;
	}
}

void Editor::Resizing::transform( const Eigen::Vector3f &relativeMovement, bool fixCenter, bool invertMask ) {
	const Eigen::Vector3f &localMask = invertMask ? invertedMask : mask;

	OBB objectOBB = editor->transformer->getOBB();

	const Vector3f boxDelta =
		objectOBB.transformation.inverse().linear() *
		editor->getScaledRelativeViewMovement( relativeMovement );
	objectOBB.size += boxDelta.cwiseProduct( localMask );
	objectOBB.size = objectOBB.size.cwiseMax( Vector3f::Constant( 1.0 ) );

	if( !fixCenter ) {
		const Eigen::Vector3f centerShift = boxDelta.cwiseProduct( localMask.cwiseAbs() ) / 2;
		objectOBB.transformation = objectOBB.transformation * Eigen::Translation3f( centerShift );
	}

	editor->transformer->setOBB( objectOBB );
}

void Editor::Resizing::onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
	if( !selected ) {
		return;
	}

	if( dragging ) {
		const sf::Vector2i draggedDelta = popMouseDelta();

		// TODO: get camera viewport size
		Eigen::Vector3f relativeMovement( draggedDelta.x, -draggedDelta.y, 0.0f );
		relativeMovement *= transformSpeed;

		if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
			relativeMovement *= 4;
		}

		transform( relativeMovement, sf::Keyboard::isKeyPressed( sf::Keyboard::LControl ), sf::Keyboard::isKeyPressed( sf::Keyboard::LAlt ) );
	}
}

void Editor::Resizing::render() {
	DebugRender::begin();
	DebugRender::setPosition( hitPoint );
	DebugRender::drawAbstractSphere( 0.5 );
	DebugRender::end();
}
