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
#if 0
struct Editor : VerboseEventHandler {
	OOB oob;

	struct Mode : VerboseEventHandler {
		virtual void render() {}
	};

	struct Selecting : VerboseEventHandler {
		Editing *parent;

		bool handleEvent( const sf::Event &event ) {
		}
	};
	struct Placing : VerboseEventHandler {

	};

	struct Moving : VerboseEventDispatcher {
		Editing *parent;

		Eigen::Matrix3f viewToWorldMatrix;

		Eigen::Affine3f original_objectToWorld;

		sf::Vector2i dragging_startPosition;
		bool dragging;

		void init() {
			dragging = false;
		}

		void save() {
			original_objectToWorld = parent->oob.transformation;
		}

		void restore() {
			parent->oob.transformation = original_objectToWorld;
		}

		//std::shared_ptr<Camera> camera;
		float moveSpeed;

		void init() {
			super::init( window );
			//this->camera = camera;
			this->moveSpeed = 10.0f;
		}

		bool handleEvent( const sf::Event &event ) {
			if( super::handleEvent( event ) ) {
				return true;
			}

			switch( event.type ) {
			case sf::Event::MouseWheelMoved:
				moveSpeed *= std::pow( 1.5f, (float) event.mouseWheel.delta );
				return true;
				break;
			case sf::Event::KeyPressed:
				if( event.key.code == sf::Keyboard::Escape ) {
					dragging = false;
					restore();
					return true;
				}
				break;
			case sf::Event::MouseButtonPressed:
				if( event.mouseButton.button == sf::Mouse::Button::Left ) {
					save();
					dragging = true;
					dragging_startPosition = sf::Mouse::getPosition();
					return true;
				}
			case sf::Event::MouseButtonReleased:
				if( event.mouseButton.button == sf::Mouse::Button::Left ) {
					if( dragging ) {
						dragging = false;
						// accept
						save();
						return true;
					}
				}
				break;
			}
			if( dragging ) {
				return true;
			}
			return false;
		}

		bool update( const float elapsedTime, bool inputProcessed ) {
			if( inputProcessed ) {
				return false;
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

				relativeMovement *= elapsedTime * moveSpeed;
				if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
					relativeMovement *= 4;
				}

				const auto translation = Eigen::Translation3f( viewToWorldMatrix * relativeMovement );
				oob.transformation = translation * oob.transformation;

				return true;
			}
			else {
				sf::Vector2i draggedDelta =  sf::Mouse::getPosition() - dragging_startPosition;

				// TODO: get camera viewport size
				Eigen::Vector3f relativeMovement( draggedDelta.x, draggedDelta.y, 0.0 );
				relativeMovement /= 10.0;

				if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
					relativeMovement *= 4;
				}

				const auto translation = Eigen::Translation3f( viewToWorldMatrix * relativeMovement );
				oob.transformation = translation * oob.transformation;

				return true;
			}

			return false;
		}

		std::string getHelp(const std::string &prefix /* = std::string */ ) {
			return prefix + "click+drag with mouse to move, and WASD, Space and Ctrl for precise moving; keep shift pressed for faster movement\n";
		}
	};

	struct Rotating : VerboseEventHandler {

	};

	struct Resizing : VerboseEventHandler {

	};

	VerboseEventDispatcher dispatcher;
	VerboseEventRouter router;

	Selecting selecting;
	Placing placing;
	Moving moving;
	Rotating rotating;
	Resizing resizing;

	Mode *target;

	Editor() : target( nullptr ), VerboseEventHandler( "Editor" ) {}

	void init() {
		dispatcher.eventHandlers.push_back( make_nonallocated_shared( router ) );

		dispatcher.eventHandlers.push_back( new KeyAction( "enter selection mode", sf::Keyboard::F6, [&] () {
			router.target = target = &selecting;
		} ) );
		dispatcher.eventHandlers.push_back( new KeyAction( "enter placement mode", sf::Keyboard::F7, [&] () {
			router.target = target = &placing;
		} ) );
		dispatcher.eventHandlers.push_back( new KeyAction( "enter movement mode", sf::Keyboard::F8, [&] () {
			router.target = target = &moving;
		} ) );
		dispatcher.eventHandlers.push_back( new KeyAction( "enter rotation mode", sf::Keyboard::F9, [&] () {
			router.target = target = &rotating;
		} ) );
		dispatcher.eventHandlers.push_back( new KeyAction( "enter resize mode", sf::Keyboard::F10, [&] () {
			router.target = target = &resizing;
		} ) );
		dispatcher.eventHandlers.push_back( new KeyAction( "enter free-look mode", sf::Keyboard::F12, [&] () {
			router.target = target = &resizing;
		} ) );
	
		router.eventHandlers.push_back( make_nonallocated_shared( selecting ) );
		router.eventHandlers.push_back( make_nonallocated_shared( placing ) );
		router.eventHandlers.push_back( make_nonallocated_shared( moving ) );
		router.eventHandlers.push_back( make_nonallocated_shared( rotating ) );
		router.eventHandlers.push_back( make_nonallocated_shared( resizing ) );
	}

	void render {
		DebugRender::begin();
		DebugRender::setTransformation( oob.transformation );
		DebugRender::drawBox( oob.size );
		DebugRender::end();

		if( target ) {
			target->render();
		}
	}

};
#endif

void real_main() {
	sf::RenderWindow window( sf::VideoMode( 640, 480 ), "sgsSceneViewer", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
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

	SGSSceneRenderer sgsSceneRenderer;
	OptixRenderer optixRenderer;
	SGSScene sgsScene;
	RenderContext renderContext;
	renderContext.setDefault();

	{
		boost::timer::auto_cpu_timer timer( "SGSSceneRenderer: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		sgsSceneRenderer.reloadShaders();

		const char *scenePath = "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene";
		{
			Serializer::BinaryReader reader( scenePath );
			Serializer::read( reader, sgsScene );
		}

		const char *cachePath = "scene.sgsRendererCache";
		sgsSceneRenderer.processScene( make_nonallocated_shared( sgsScene ), cachePath );
	}
	{
		boost::timer::auto_cpu_timer timer( "OptixRenderer: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		optixRenderer.init( make_nonallocated_shared( sgsSceneRenderer ) );
	}

	EventDispatcher eventDispatcher( "Root:" );
	eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( cameraInputControl ) );

	EventSystem eventSystem;
	eventSystem.rootHandler = make_nonallocated_shared( eventDispatcher );
	eventSystem.exclusiveMode.window = make_nonallocated_shared( window );
	
	EventDispatcher verboseEventDispatcher( "sub" );
	eventDispatcher.addEventHandler( make_nonallocated_shared( verboseEventDispatcher ) );

	registerConsoleHelpAction( eventDispatcher );

	KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { sgsSceneRenderer.reloadShaders(); } );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( reloadShadersAction ) );

	BoolVariableToggle showBoundingSpheresToggle( "show bounding spheres",sgsSceneRenderer.debug.showBoundingSpheres, sf::Keyboard::B );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showBoundingSpheresToggle ) );

	BoolVariableToggle showTerrainBoundingSpheresToggle( "show terrain bounding spheres",sgsSceneRenderer.debug.showTerrainBoundingSpheres, sf::Keyboard::N );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showTerrainBoundingSpheresToggle ) );

	BoolVariableToggle updateRenderListsToggle( "updateRenderLists",sgsSceneRenderer.debug.updateRenderLists, sf::Keyboard::C );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( updateRenderListsToggle ) );

	IntVariableControl disabledObjectIndexControl( "disabledModelIndex", renderContext.disabledModelIndex, -1, sgsScene.modelNames.size(), sf::Keyboard::Numpad7, sf::Keyboard::Numpad1 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledObjectIndexControl ) );

	IntVariableControl disabledInstanceIndexControl( "disabledInstanceIndex",renderContext.disabledInstanceIndex, -1, sgsScene.numSceneObjects, sf::Keyboard::Numpad9, sf::Keyboard::Numpad3 );
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
			sgsSceneRenderer.renderShadowmap( renderContext );

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// OpenGL drawing commands go here...
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( camera.getProjectionMatrix() );
			glMultMatrix( camera.getViewTransformation().matrix() );

			glMatrixMode( GL_MODELVIEW );
			// TODO: move this into the render functions [9/23/2012 kirschan2]
			glLoadIdentity();

			sgsSceneRenderer.render( camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition(), renderContext );

			//probeVisualization.render();		

			selectObjectsByModelID( sgsSceneRenderer, renderContext.disabledModelIndex );
			glDisable( GL_DEPTH_TEST );
			selectionDR.render();
			glEnable( GL_DEPTH_TEST );

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