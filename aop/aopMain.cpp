#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <boost/timer/timer.hpp>
#include <iterator>

#include <Eigen/Eigen>
#include <GL/glew.h>
#include <unsupported/Eigen/OpenGLSupport>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Debug.h"

#include <memory>

using namespace Eigen;

#include "camera.h"
#include "cameraInputControl.h"
#include <verboseEventHandlers.h>

#include "make_nonallocated_shared.h"

#include "sgsSceneRenderer.h"
#include "optixRenderer.h"

//#include "debugWindows.h"

#include "sgsInterface.h"
#include "mathUtility.h"
#include "optixEigenInterop.h"
#include "grid.h"
#include "probeGenerator.h"
#include "mathUtility.h"

//#include "candidateFinderInterface.h"

void visualizeProbes( float resolution, const std::vector< SGSInterface::Probe > &probes );

DebugRender::CombinedCalls selectionDR;

void selectObjectsByModelID( SGSSceneRenderer &renderer, int modelIndex ) {
	SGSSceneRenderer::InstanceIndices indices = renderer.getModelInstances( modelIndex );

	selectionDR.begin();
	selectionDR.setColor( Eigen::Vector3f::UnitX() );

	for( auto instanceIndex = indices.begin() ; instanceIndex != indices.end() ; ++instanceIndex ) {
		auto transformation = renderer.getInstanceTransformation( *instanceIndex );
		auto boundingBox = renderer.getUntransformedInstanceBoundingBox( *instanceIndex );

		selectionDR.setTransformation( transformation );
		selectionDR.drawAABB( boundingBox.min(), boundingBox.max() );
	}

	selectionDR.end();
}

DebugRender::CombinedCalls probeDumps;

void sampleInstances( OptixRenderer &optix, SGSSceneRenderer &renderer, int modelIndex ) {
	SGSSceneRenderer::InstanceIndices indices = renderer.getModelInstances( modelIndex );

	std::vector< OptixRenderer::ProbeContext > probeContexts;
	std::vector< OptixRenderer::Probe > probes;

	for( int instanceIndexIndex = 0 ; instanceIndexIndex < indices.size() ; ++instanceIndexIndex ) {
		const auto &instanceIndex = indices[ instanceIndexIndex ];

		RenderContext renderContext;
		renderContext.disabledModelIndex = -1;
		renderContext.disabledInstanceIndex = instanceIndex;

		// create probes
		std::vector< OptixRenderer::Probe > localProbes;

		const float resolution = 1;
		auto boundingBox = renderer.getUntransformedInstanceBoundingBox( instanceIndex );
		auto transformation = Eigen::Affine3f( renderer.getInstanceTransformation( instanceIndex ) );
		auto mapping = createIndexMapping( ceil( boundingBox.sizes() / resolution ),boundingBox.min(), resolution );

		auto center = transformation * boundingBox.center();
		for( auto iterator = mapping.getIterator() ; iterator.hasMore() ; ++iterator ) {
			OptixRenderer::Probe probe;
			auto probePosition = Eigen::Vector3f::Map( &probe.position.x );
			auto probeDirection = Eigen::Vector3f::Map( &probe.direction.x );
			probePosition = transformation * mapping.getPosition( iterator.getIndex3() );
			probeDirection = (probePosition - center).normalized();
			localProbes.push_back( probe );
		}

		std::vector< OptixRenderer::ProbeContext > localProbeContexts;
		optix.sampleProbes( localProbes, localProbeContexts, renderContext );

		boost::push_back( probes, localProbes );
		boost::push_back( probeContexts, localProbeContexts );
	}

	probeDumps.begin();
	for( int probeContextIndex = 0 ; probeContextIndex < probeContexts.size() ; ++probeContextIndex ) {
		const auto &probeContext = probeContexts[ probeContextIndex ];
		probeDumps.setPosition( Eigen::Vector3f::Map( &probes[ probeContextIndex ].position.x ) );
		glColor4ubv( &probeContext.color.x );
		probeDumps.drawVectorCone( probeContext.distance * Eigen::Vector3f::Map( &probes[ probeContextIndex ].direction.x ), probeContexts.front().distance * 0.25, 1 + float( probeContext.hitCounter ) / OptixProgramInterface::numProbeSamples * 15 );
	}
	probeDumps.end();
}

#if 1

struct MouseDelta {
	sf::Vector2i lastPosition;
	
	void reset() {
		lastPosition = sf::Mouse::getPosition();
	}

	sf::Vector2i pop() {
		const sf::Vector2i currentPosition = sf::Mouse::getPosition();
		const sf::Vector2i delta = currentPosition - lastPosition;
		lastPosition = currentPosition;
		return delta;
	}
};

struct Editor : EventDispatcher {
	OBB obb;
	Camera *camera;

	struct Mode : NullEventHandler {
		Editor *editor;

		bool dragging;
		bool selected;
		//MouseDelta dragDelta;

		Mode( Editor *editor ) : editor( editor ), dragging( false ), selected( false ) {}

		sf::Vector2i popMouseDelta() {
			return eventSystem.exclusiveMode.popMouseDelta();
		}

		virtual void render() {}

		virtual void storeState() {}

		virtual void restoreState() {}
		
		void startDragging() {
			if( dragging ) {
				return;
			}

			storeState();
			dragging = true;
			//dragDelta.reset();
			eventSystem.setCapture( this, FT_EXCLUSIVE );
		}

		void stopDragging( bool accept ) {
			if( !dragging ) {
				return;
			}

			if( accept ) {
				storeState();
			}
			else {
				restoreState();
			}

			dragging = false;
			eventSystem.setCapture( nullptr, FT_EXCLUSIVE );
		}

		void onSelected() {
			selected = true;
		}

		void onUnselected() {
			selected = false;
			stopDragging( false );
		}

		bool acceptFocus( FocusType focusType ) {
			return true;
		}
	};

	struct TransformMode : Mode {
		float transformSpeed;
		OBB::Transformation storedTransformation;

		virtual void transform( const Eigen::Vector3f &relativeMovement ) = 0;

		TransformMode( Editor *editor ) : Mode( editor ), transformSpeed( 1.0f ) {}

		void storeState() {
			storedTransformation = editor->obb.transformation;
		}

		void restoreState() {
			editor->obb.transformation = storedTransformation;
		}

		virtual void onMouse( EventState &eventState ) {
			switch( eventState.event.type ) {
			case sf::Event::MouseWheelMoved:
				transformSpeed *= std::pow( 1.5f, (float) eventState.event.mouseWheel.delta );
				eventState.accept();
				break;
			case sf::Event::MouseButtonPressed:
				if( eventState.event.mouseButton.button == sf::Mouse::Button::Left ) {
					startDragging();
					eventState.accept();
				}
				break;
			case sf::Event::MouseButtonReleased:
				if( eventState.event.mouseButton.button == sf::Mouse::Button::Left ) {
					stopDragging( true );
					eventState.accept();
				}
				break;
			}
			if( dragging ) {
				eventState.accept();
			}
		}

		void onKeyboard( EventState &eventState ) {
			switch( eventState.event.type ) {
			case sf::Event::KeyPressed:
				if( eventState.event.key.code == sf::Keyboard::Escape && dragging ) {
					stopDragging( false );
					eventState.accept();
				}
				break;
			}
		}

		void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
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
				relativeMovement *= 0.1;

				transform( relativeMovement );
			}
		}

		void onNotify( const EventState &eventState ) {
			if( eventState.event.type == sf::Event::LostFocus ) {
				stopDragging( false );
			}
		}

		std::string getHelp(const std::string &prefix /* = std::string */ ) {
			return prefix + "click+drag with mouse to transform, and WASD, Space and Ctrl for precise transformation; keep shift pressed for faster transformation; use the mouse wheel to change precise transformation granularity\n";
		}
	};

	struct Selecting : Mode {
		Selecting( Editor *editor ) : Mode( editor ) {}
	};

	struct Placing : Mode {
		Placing( Editor *editor ) : Mode( editor ) {}
	};

	struct Moving : TransformMode {
		Moving( Editor *editor ) : TransformMode( editor ) {}

		virtual void transform( const Eigen::Vector3f &relativeMovement ) {
			const auto translation = Eigen::Translation3f( editor->camera->getViewRotation().transpose() * relativeMovement );
			editor->obb.transformation = translation * editor->obb.transformation;
		}
	};

	struct Rotating : TransformMode {
		Rotating( Editor *editor ) : TransformMode( editor ) {}

		void transform( const Eigen::Vector3f &relativeMovement ) {
			const Vector3f offset = editor->obb.transformation.translation();

			const auto rotation =
				Eigen::AngleAxisf( relativeMovement.z(), Vector3f::UnitZ() ) *
				Eigen::AngleAxisf( -relativeMovement.y(), Vector3f::UnitX() ) *
				Eigen::AngleAxisf( relativeMovement.x(), Vector3f::UnitY());

			editor->obb.transformation = Eigen::Translation3f( offset ) * rotation * Eigen::Translation3f( -offset ) * editor->obb.transformation;
		}
	};

	struct Resizing : Mode {
		OBB storedOBB;
		Eigen::Vector3f mask, hitPoint;

		float transformSpeed;

		Resizing( Editor *editor ) : Mode( editor ), transformSpeed( 0.01 ) {}
		
		void storeState() {
			storedOBB = editor->obb;
		}

		void restoreState() {
			editor->obb = storedOBB;
		}

		bool setCornerMasks( int x, int y ) {
			// TODO: get window/viewport size [9/28/2012 kirschan2]
			// camera property?
			const sf::Vector2i size( eventSystem.exclusiveMode.window->getSize() );
			const float xh = float( x ) / size.x * 2 - 1;
			const float yh = -(float( y ) / size.y * 2 - 1);

			const Eigen::Vector4f nearPlanePoint( xh, yh, -1.0, 1.0 );
			const Eigen::Vector3f direction = ((editor->camera->getProjectionMatrix() * editor->camera->getViewTransformation().matrix()).inverse() * nearPlanePoint).hnormalized() - editor->camera->getPosition();
			
			//Eigen::Vector3f hitPoint;
			if( intersectRayWithOBB( editor->obb, editor->camera->getPosition(), direction, hitPoint ) ) {
				const Eigen::Vector3f boxHitPoint = editor->obb.transformation.inverse() * hitPoint;

				// determine the nearest plane, edge whatever
				const Eigen::Vector3f p = boxHitPoint.cwiseQuotient( editor->obb.size / 2.0 );

				// TODO: if we click near the center of a face we should behave just like below [9/28/2012 kirschan2]
				int maskFlag = 0;
				for( int i = 0 ; i < 3 ; i++ ) {
					if( fabs( p[i] ) > 0.9 ) {
						maskFlag |= 7 - (1<<i);
					}	
				}

				mask.setZero();
				for( int i = 0 ; i < 3 ; i++ ) {
					if( maskFlag & (1<<i) ) {
						mask[i] = (p[i] > 0.0) * 2 - 1;
					}
				}
			}
			else {
				const auto objectDirection = editor->obb.transformation.translation() - editor->camera->getPosition();
				hitPoint = editor->camera->getPosition() + 
					direction.normalized() / direction.normalized().dot( objectDirection ) * objectDirection.squaredNorm();
				hitPoint = editor->obb.transformation.inverse() * hitPoint;
				const Eigen::Vector3f boxHitPoint = nearestPointOnAABoxToPoint( -editor->obb.size / 2, editor->obb.size / 2, hitPoint );
				//hitPoint = editor->obb.transformation * boxHitPoint;

				// determine the nearest plane, edge whatever
				const Eigen::Vector3f p = boxHitPoint.cwiseQuotient( editor->obb.size / 2.0 );

				int maskFlag = 0;
				for( int i = 0 ; i < 3 ; i++ ) {
					if( fabs( p[i] ) > 0.9 ) {
						maskFlag |= 1<<i;
					}	
				}

				mask.setZero();
				for( int i = 0 ; i < 3 ; i++ ) {
					if( maskFlag & (1<<i) ) {
						mask[i] = (p[i] > 0.0) * 2 - 1;
					}
				}
			}			

			return true;
		}

		void onMouse( EventState &eventState ) {
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

		void onKeyboard( EventState &eventState ) {
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

		void transform( const Eigen::Vector3f &relativeMovement, bool fixCenter ) {
			const Vector3f boxDelta = editor->obb.transformation.inverse().linear() * editor->camera->getViewRotation().transpose() * relativeMovement;
			editor->obb.size += boxDelta.cwiseProduct( mask );
			editor->obb.size = editor->obb.size.cwiseMax( Vector3f::Constant( 1.0 ) );
			
			if( !fixCenter ) {
				const Vector3f centerShift = boxDelta.cwiseProduct( mask.cwiseAbs() ) / 2;
				editor->obb.transformation = editor->obb.transformation * Eigen::Translation3f( centerShift );
			}
		}

		void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
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

				transform( relativeMovement, sf::Keyboard::isKeyPressed( sf::Keyboard::LControl ) );
			}
		}

		void render() {
			DebugRender::begin();
			DebugRender::setPosition( hitPoint );
			DebugRender::drawAbstractSphere( 0.5 );
			DebugRender::end();
		}
	};

	EventDispatcher dispatcher;
	TemplateEventRouter<Mode> modes;

	Selecting selecting;
	Placing placing;
	Moving moving;
	Rotating rotating;
	Resizing resizing;

	Editor() 
		: 
			EventDispatcher( "Editor" ),
			modes( "Mode" ),
			dispatcher( "" ),
			selecting( this ),
			placing( this ),
			moving( this ),
			rotating( this ),
			resizing( this )
			 {}

	void selectMode( Mode *mode ) {
		modes.setTarget( mode );
		eventSystem.setCapture( mode,  FT_KEYBOARD );
	}

	void init() {
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
			selectMode( &placing );
		} ) );
		addEventHandler( std::make_shared<KeyAction>( "enter movement mode", sf::Keyboard::F8, [&] () {
			selectMode( &moving );
		} ) );
		addEventHandler( std::make_shared<KeyAction>( "enter rotation mode", sf::Keyboard::F9, [&] () {
			selectMode( &rotating );
		} ) );
		addEventHandler( std::make_shared<KeyAction>( "enter resize mode", sf::Keyboard::F10, [&] () {
			selectMode( &resizing );
		} ) );
		addEventHandler( std::make_shared<KeyAction>( "enter free-look mode", sf::Keyboard::F5, [&] () {
			selectMode( nullptr );
		} ) );

		obb.transformation.setIdentity();
		obb.size.setConstant( 3.0 );
	}

	void render() {
		DebugRender::begin();
		DebugRender::setTransformation( obb.transformation );
		DebugRender::drawBox( obb.size );
		DebugRender::end();

		if( modes.target ) {
			modes.target->render();
		}
	}
};
#endif

EventSystem eventSystem;

void real_main() {
	sf::RenderWindow window( sf::VideoMode( 640, 480 ), "AOP", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
	glewInit();

	glutil::RegisterDebugOutput( glutil::STD_OUT );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 0.05;
	camera.perspectiveProjectionParameters.zFar = 500.0;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( make_nonallocated_shared(camera) );

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	SGSInterface::World world;
	SGSInterface::View view;
	view.renderContext.setDefault();

	const char *scenePath = "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene";
	//world.init( scenePath );

	EventDispatcher eventDispatcher( "Root:" );
	eventDispatcher.addEventHandler( make_nonallocated_shared( cameraInputControl ) );
	
	registerConsoleHelpAction( eventDispatcher );

	eventSystem.rootHandler = make_nonallocated_shared( eventDispatcher );
	eventSystem.exclusiveMode.window = make_nonallocated_shared( window );

	EventDispatcher verboseEventDispatcher( "sub" );
	eventDispatcher.addEventHandler( make_nonallocated_shared( verboseEventDispatcher ) );

	Editor editor;
	editor.camera = &camera;
	editor.init();
	
	eventDispatcher.addEventHandler( make_nonallocated_shared( editor ) );

	KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { world.sceneRenderer.reloadShaders(); } );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( reloadShadersAction ) );

	BoolVariableToggle showBoundingSpheresToggle( "show bounding spheres", world.sceneRenderer.debug.showBoundingSpheres, sf::Keyboard::B );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showBoundingSpheresToggle ) );

	BoolVariableToggle showTerrainBoundingSpheresToggle( "show terrain bounding spheres",world.sceneRenderer.debug.showTerrainBoundingSpheres, sf::Keyboard::N );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showTerrainBoundingSpheresToggle ) );

	BoolVariableToggle updateRenderListsToggle( "updateRenderLists",world.sceneRenderer.debug.updateRenderLists, sf::Keyboard::C );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( updateRenderListsToggle ) );

	IntVariableControl disabledObjectIndexControl( "disabledModelIndex", view.renderContext.disabledModelIndex, -1, world.scene.modelNames.size(), sf::Keyboard::Numpad7, sf::Keyboard::Numpad1 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledObjectIndexControl ) );

	IntVariableControl disabledInstanceIndexControl( "disabledInstanceIndex",view.renderContext.disabledInstanceIndex, -1, world.scene.numSceneObjects, sf::Keyboard::Numpad9, sf::Keyboard::Numpad3 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledInstanceIndexControl ) );

	//DebugWindowManager debugWindowManager;

#if 0
	TextureVisualizationWindow optixWindow;
	optixWindow.init( "Optix Version" );
	optixWindow.texture = optixRenderer.debugTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( optixWindow ) );
#endif
#if 0
	TextureVisualizationWindow mergedTextureWindow;
	mergedTextureWindow.init( "merged object textures" );
	mergedTextureWindow.texture = sgsSceneRenderer.mergedTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( mergedTextureWindow ) );
#endif

	sf::Text renderDuration;
	renderDuration.setPosition( 0, 0 );
	renderDuration.setCharacterSize( 10 );

	Instance testInstance;
	testInstance.modelId = 1;
	testInstance.transformation.setIdentity();
	//sgsSceneRenderer.addInstance( testInstance );

	ProbeGenerator::initDirections();

#if 0
	renderContext.disabledModelIndex = 0;
	DebugRender::DisplayList probeVisualization;
	{
		auto instanceIndices = sgsSceneRenderer.getModelInstances( 0 );

		int totalCount = 0;

		for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
			boost::timer::auto_cpu_timer timer( "ProbeSampling, batch: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

			std::vector<SGSInterface::Probe> probes, transformedProbes;

			SGSInterface::generateProbes( *instanceIndex, 0.25, sgsSceneRenderer, probes, transformedProbes );

			std::vector< OptixRenderer::ProbeContext > probeContexts;

			std::cout << "sampling " << transformedProbes.size() << " probes in one batch:\n\t";
			//optixRenderer.sampleProbes( transformedProbes, probeContexts, renderContext );

			totalCount += transformedProbes.size();

			probeVisualization.beginCompileAndAppend();
			visualizeProbes( 0.25, transformedProbes );
			probeVisualization.endCompile();
		}

		std::cout << "num probes: " << totalCount << "\n";
	}
#endif

	while (true)
	{
		// Activate the window for OpenGL rendering
		window.setActive();

		// Event processing
		sf::Event event;
		while (window.pollEvent(event))
		{
			// Request for closing the window
			if (event.type == sf::Event::Closed)
				window.close();

			if( event.type == sf::Event::Resized ) {
				camera.perspectiveProjectionParameters.aspect = float( event.size.width ) / event.size.height;
				glViewport( 0, 0, event.size.width, event.size.height );
			}

			eventSystem.processEvent( event );
		}

		if( !window.isOpen() ) {
			break;
		}

		eventSystem.update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );

		{
			boost::timer::cpu_timer renderTimer;

			view.updateFromCamera( camera );
			//world.renderViewFrame( view );

			//probeVisualization.render();

			/*selectObjectsByModelID( world.sceneRenderer, view.renderContext.disabledModelIndex );
			glDisable( GL_DEPTH_TEST );
			selectionDR.render();
			glEnable( GL_DEPTH_TEST );*/

			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( view.viewerContext.projectionView );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			editor.render();

			//const ViewerContext viewerContext = { camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() };
			//optixRenderer.renderPinholeCamera( viewerContext, renderContext );

			renderDuration.setString( renderTimer.format() );
			window.pushGLStates();
			window.resetGLStates();
			window.draw( renderDuration );
			window.popGLStates();

			// End the current frame and display its contents on screen
			window.display();

			//debugWindowManager.update();
		}

	}
};

void main() {
	try {
		real_main();
	}
	catch( std::exception &e) {
		std::cout << e.what() << "\n";
	}
}