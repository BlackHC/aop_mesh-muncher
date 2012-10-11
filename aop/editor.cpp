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
		selectMode( M_SELECTING );
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter placement mode", sf::Keyboard::F7, [&] () {
		if( selection && selection->canTransform() ) {
			selectMode( M_PLACING );
		}
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter movement mode", sf::Keyboard::F8, [&] () {
		if( selection && selection->canTransform() ) {
			selectMode( M_MOVING );
		}
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter rotation mode", sf::Keyboard::F9, [&] () {
		if( selection && selection->canTransform() ) {
			selectMode( M_ROTATING );
		}
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter resize mode", sf::Keyboard::F10, [&] () {
		if( selection && selection->canResize() ) {
			selectMode( M_RESIZING );
		}
	} ) );
	addEventHandler( std::make_shared<KeyAction>( "enter free-look mode", sf::Keyboard::F5, [&] () {
		selectMode( M_FREELOOK );
	} ) );
}

void Editor::renderHighlitOBB( const Obb &obb ) {
	DebugRender::begin();
	DebugRender::setTransformation( obb.transformation );

	for( int pass = 0 ; pass < 2 ; pass++ ) {
		switch( pass ) {
		case 0:
			glDepthMask( GL_FALSE );
			glDepthFunc( GL_GEQUAL );
			DebugRender::setColor( Eigen::Vector3f::UnitX() * 0.5f );
			break;
		case 1:
			glDepthMask( GL_TRUE );
			glDepthFunc( GL_LEQUAL );
			DebugRender::setColor( Eigen::Vector3f::UnitX() );
			break;
		}

		if( !obb.size.isZero() ) {
			DebugRender::drawBox( obb.size );
		}
		else {
			DebugRender::drawAbstractSphere( 0.5 );
		}
	}

	DebugRender::end();

	// reset the state
	glDepthFunc( GL_LESS );
}

void Editor::render() {
	if( selection ) {
		selection->render();
	}

	if( modes.target ) {
		modes.target->render();
	}
}

void Editor::selectMode( ModeState newMode ) {
	Mode *mode = nullptr;

	switch( newMode ) {
	case M_FREELOOK:
		break;
	case M_SELECTING:
		mode = &selecting;
		break;
	case M_PLACING:
		if( selection && selection->canTransform() ) {
			mode = &placing;
		}
		break;
	case M_MOVING:
		if( selection && selection->canTransform() ) {
			mode = &moving;
		}
		break;
	case M_ROTATING:
		if( selection && selection->canTransform() ) {
			mode = &rotating;
		}
		break;
	case M_RESIZING:
		if( selection && selection->canResize() ) {
			mode = &resizing;
		}
		break;
	}

	currentMode = newMode;
	modes.setTarget( mode );
	getEventSystem()->setCapture( mode,  FT_KEYBOARD );
}

void Editor::convertScreenToHomogeneous( int x, int y, float &xh, float &yh ) {
	// TODO: refactor [9/30/2012 kirschan2]
	const sf::Vector2i size( getEventSystem()->exclusiveMode.window->getSize() );
	xh = float( x ) / size.x * 2 - 1;
	yh = -(float( y ) / size.y * 2 - 1);
}

Eigen::Vector3f Editor::getScaledRelativeViewMovement( const Eigen::Vector3f &relativeMovement ) {
	Eigen::Vector3f u, v, w;
	Eigen::unprojectAxes( view->viewerContext.worldViewerPosition, view->viewerContext.projectionView, u, v, w );

	const float scale = w.dot( selection->getTransformation().translation() - view->viewerContext.worldViewerPosition ) / w.squaredNorm();

	return
		u * scale * relativeMovement.x() +
		v * scale * relativeMovement.y() +
		w * scale * relativeMovement.z()
		;
}

void Editor::TransformMode::storeState() {
	storedTransformation = editor->selection->getTransformation();
}

void Editor::TransformMode::restoreState() {
	editor->selection->setTransformation( storedTransformation );
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

	bool localMode = sf::Keyboard::isKeyPressed( sf::Keyboard::LAlt );

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

		transform( relativeMovement, localMode );
	}
	else {
		const sf::Vector2i draggedDelta = popMouseDelta();

		// TODO: get camera viewport size
		Eigen::Vector3f relativeMovement( draggedDelta.x, -draggedDelta.y, 0.0 );

		if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
			relativeMovement *= 4;
		}
		relativeMovement *= 0.01;

		transform( relativeMovement, localMode );
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

void Editor::Selecting::onKeyboard( EventState &eventState ) {
	switch( eventState.event.key.code ) {
	case sf::Keyboard::Escape:
		editor->selection.reset();
		eventState.accept();
		break;
	case sf::Keyboard::LShift:
	case sf::Keyboard::LAlt:
	case sf::Keyboard::LControl:
		eventState.accept();
		break;
	case sf::Keyboard::Delete:
		if( eventState.event.type == sf::Event::KeyReleased ) {
			struct DeleteVisitor : SelectionVisitor {
				Editor *editor;

				DeleteVisitor( Editor *editor ) : editor( editor ) {}

				void visit( SGSInstanceSelection *instanceSelection ) {
					editor->world->removeInstance( instanceSelection->instanceIndex );
					editor->deselect();
				}
			};
			DeleteVisitor( editor ).dispatch( editor->selection );
		}
		eventState.accept();
		break;
	}
}

void Editor::Selecting::onMouse( EventState &eventState ) {
	bool selectModel = sf::Keyboard::isKeyPressed( sf::Keyboard::LAlt );
	bool inclusiveToggleSelection = sf::Keyboard::isKeyPressed( sf::Keyboard::LShift );
	bool removeFromSelection = sf::Keyboard::isKeyPressed( sf::Keyboard::LControl );

	if( eventState.event.type == sf::Event::MouseButtonPressed && eventState.event.mouseButton.button == sf::Mouse::Left ) {
		if( !inclusiveToggleSelection && !removeFromSelection ) {
			editor->selection.reset();
		}

		float xh, yh;
		editor->convertScreenToHomogeneous( eventState.event.mouseButton.x, eventState.event.mouseButton.y, xh, yh );

		// duplicate from resizing [9/30/2012 kirschan2]
		const Eigen::Vector4f nearPlanePoint( xh, yh, -1.0, 1.0 );
		const Eigen::Vector3f direction = ((editor->view->viewerContext.projectionView.inverse() * nearPlanePoint).hnormalized() - editor->view->viewerContext.worldViewerPosition).normalized();

		float bestT;
		int bestOBB = -1;
		for( int obbIndex = 0 ; obbIndex < editor->volumes->getCount() ; obbIndex++ ) {
			const Obb *obb = editor->volumes->get( obbIndex );
			float t;

			if(
				intersectRayWithOBB( *obb, editor->view->viewerContext.worldViewerPosition, direction, nullptr, &t ) &&
				t > 0.0f
			) {
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

				if( !selectModel ) {
					editor->selectInstance( result.objectIndex );
				}
				else {
					struct Selector : SelectionVisitor {
						Editor *editor;
						int modelIndex;
						bool removeFromSelection;

						Selector( Editor *editor, int modelIndex, bool removeFromSelection ) :
							editor( editor ),
							modelIndex( modelIndex ),
							removeFromSelection( removeFromSelection )
						{}

						void visit() {
							editor->selectModel( modelIndex );
						}

						void visit( ISelection *selection ) {
							visit();
						}

						void visit( SGSMultiModelSelection *modelSelection ) {
							auto found = boost::find( modelSelection->modelIndices, modelIndex );
							if( !removeFromSelection && found == modelSelection->modelIndices.end() ) {
								modelSelection->modelIndices.push_back( modelIndex );
							}
							else if( found != modelSelection->modelIndices.end() ) {
								modelSelection->modelIndices.erase( found );
							}
						}
					};
					Selector( editor, result.modelIndex, removeFromSelection ).dispatch( editor->selection );
				}
			}
		}

		if( bestOBB != -1 ) {
			editor->selectObb( bestOBB );
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
			const auto transformation = editor->selection->getTransformation();
			editor->selection->setTransformation(
				Eigen::Translation3f( Eigen::map( result.hitPosition) ) * transformation.linear()
			);
		}
	}
	eventState.accept();
}

void Editor::Moving::transform( const Eigen::Vector3f &relativeMovement, bool localMode ) {
	if( !localMode ) {
		const auto scaledRelativeMovement = editor->getScaledRelativeViewMovement( relativeMovement );
		editor->selection->setTransformation( Eigen::Translation3f( scaledRelativeMovement ) * editor->selection->getTransformation() );
	}
	else {
		const auto transformation = editor->selection->getTransformation();
		editor->selection->setTransformation( Eigen::Translation3f( transformation.linear() * relativeMovement ) * transformation );
	}
}

void Editor::Rotating::transform( const Eigen::Vector3f &relativeMovement, bool localMode ) {
	Eigen::Affine3f rotation;

	const auto transformation = editor->selection->getTransformation();

	if( !localMode ) {
		rotation =
			Eigen::AngleAxisf( relativeMovement.z(), editor->view->viewAxes.row(2) ) *
			Eigen::AngleAxisf( relativeMovement.y(), editor->view->viewAxes.row(0) ) *
			Eigen::AngleAxisf( relativeMovement.x(), editor->view->viewAxes.row(1) )
		;
	}
	else {
		rotation =
			Eigen::AngleAxisf( relativeMovement.z(), transformation.linear().col(2) ) *
			Eigen::AngleAxisf( relativeMovement.y(), transformation.linear().col(0) ) *
			Eigen::AngleAxisf( relativeMovement.x(), transformation.linear().col(1) )
		;
	}

	const Vector3f translation = editor->selection->getTransformation().translation();

	editor->selection->setTransformation(
		Eigen::Translation3f( translation ) *
		rotation *
		Eigen::Translation3f( -translation ) *
		transformation
	);
}

void Editor::Resizing::storeState() {
	storedOBB = editor->selection->getObb();
}

void Editor::Resizing::restoreState() {
	editor->selection->setObb( storedOBB );
}

bool Editor::Resizing::setCornerMasks( int x, int y ) {
	// TODO: get window/viewport size [9/28/2012 kirschan2]
	// camera property?
	float xh, yh;
	editor->convertScreenToHomogeneous( x, y, xh, yh );

	const Eigen::Vector4f nearPlanePoint( xh, yh, -1.0, 1.0 );
	const Eigen::Vector3f direction = (editor->view->viewerContext.projectionView.inverse() * nearPlanePoint).hnormalized() - editor->view->viewerContext.worldViewerPosition;

	const Obb objectOBB = editor->selection->getObb();

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

	Obb objectOBB = editor->selection->getObb();

	const Vector3f boxDelta =
		objectOBB.transformation.inverse().linear() *
		editor->getScaledRelativeViewMovement( relativeMovement );
	objectOBB.size += boxDelta.cwiseProduct( localMask );
	objectOBB.size = objectOBB.size.cwiseMax( Vector3f::Constant( 1.0 ) );

	if( !fixCenter ) {
		const Eigen::Vector3f centerShift = boxDelta.cwiseProduct( localMask.cwiseAbs() ) / 2;
		objectOBB.transformation = objectOBB.transformation * Eigen::Translation3f( centerShift );
	}

	editor->selection->setObb( objectOBB );
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