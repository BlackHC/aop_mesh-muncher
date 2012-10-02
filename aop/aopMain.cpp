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

#include "debugWindows.h"

#include "sgsInterface.h"
#include "mathUtility.h"
#include "optixEigenInterop.h"
#include "grid.h"
#include "probeGenerator.h"
#include "mathUtility.h"

#include "editor.h"
#include "anttwbargroup.h"
#include "antTweakBarEventHandler.h"
#include "anttwbarcollection.h"

#include "autoTimer.h"
#include "candidateFinderInterface.h"
#include "aopSettings.h"


std::weak_ptr<AntTweakBarEventHandler::GlobalScope> AntTweakBarEventHandler::globalScope;

void visualizeProbes( float resolution, const std::vector< SGSInterface::Probe > &probes );

DebugRender::CombinedCalls selectionDR;

void selectObjectsByModelID( SGSSceneRenderer &renderer, int modelIndex ) {
	SGSSceneRenderer::InstanceIndices indices = renderer.getModelInstances( modelIndex );

	selectionDR.begin();
	selectionDR.setColor( Eigen::Vector3f::UnitX() );

	for( auto instanceIndex = indices.begin() ; instanceIndex != indices.end() ; ++instanceIndex ) {
		auto transformation = renderer.getInstanceTransformation( *instanceIndex );
		auto boundingBox = renderer.getUntransformedInstanceBoundingBox( *instanceIndex );

		selectionDR.setTransformation( transformation.matrix() );
		selectionDR.drawAABB( boundingBox.min(), boundingBox.max() );
	}

	selectionDR.end();
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


#endif

void sampleInstances( SGSInterface::World &world, CandidateFinder &candidateFinder, int modelIndex ) {
	AUTO_TIMER_FOR_FUNCTION();

	RenderContext renderContext;
	renderContext.setDefault();
	renderContext.disabledModelIndex = modelIndex;

	auto instanceIndices = world.sceneRenderer.getModelInstances( 0 );
	
	int totalCount = 0;

	for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
		ProbeDataset dataset;
		std::vector<SGSInterface::Probe> transformedProbes;
	
		world.generateProbes( *instanceIndex, 0.25, dataset.probes, transformedProbes );
	
		AUTO_TIMER_DEFAULT( boost::str( boost::format( "batch with %i probes" ) % transformedProbes.size() ) );
		std::cout << "sampling " << transformedProbes.size() << " probes in one batch:\n\t";
		world.optixRenderer.sampleProbes( transformedProbes, dataset.probeContexts, renderContext );
	
		candidateFinder.addDataset(modelIndex, std::move( dataset ) );
	
		totalCount += (int) transformedProbes.size();
	}
	
	std::cerr << Indentation::get() << "total sampled probes: " << totalCount << "\n";
}


// TODO: this should get its own file [9/30/2012 kirschan2]
EventSystem *EventHandler::eventSystem;

namespace aop {
	struct Application {
		Camera mainCamera;
		CameraInputControl mainCameraInputControl;

		SGSInterface::World world;
		SGSInterface::View cameraView;

		Settings settings;

		struct NamedTargetVolumeView {
			Application *application;

			typedef aop::Settings::NamedTargetVolume Type;

			NamedTargetVolumeView( Application *application ) : application( application ) {}

			template< typename Accessor >
			void create( AntTWBarUI::Container *container, Accessor &accessor ) const {
				container->add( 
					AntTWBarUI::makeSharedVariable(
					"Name", 
					AntTWBarUI::makeMemberAccessor( accessor, &aop::Settings::NamedTargetVolume::name )
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
					"Select",
					[] () { std::cout << "select called\n"; }
					)
				);
			}
		};

		struct NamedCameraStateView {
			Application *application;

			typedef aop::Settings::NamedCameraState Type;

			NamedCameraStateView( Application *application ) : application( application ) {}

			template< typename Accessor >
			void create( AntTWBarUI::Container *container, Accessor &accessor ) const {
				container->add( 
					AntTWBarUI::makeSharedVariable(
						"Name", 
						AntTWBarUI::makeMemberAccessor( accessor, &aop::Settings::NamedCameraState::name )
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Use",
						[&, this] () {
							accessor.pull().pushTo( this->application->mainCamera );							
						}
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Replace",
						[&, this] () { 
							accessor.pull().pullFrom( this->application->mainCamera );
							accessor.push();
						}
					)
				);
			}
		};

		struct MainUI {
			Application *application;
			AntTWBarUI::SimpleContainer ui;
			
			MainUI( Application *application ) : application( application ) {
				ui.setName( "aop" );
				ui.add( AntTWBarUI::makeSharedButton( "Load settings", [this] { this->application->settings.load(); } ) );
				ui.add( AntTWBarUI::makeSharedButton( "Store settings", [this] { this->application->settings.store(); } ) );
				//ui.add( AntTWBarUI::makeSharedVector< NamedTargetVolumeView >( "Camera Views", application->settings.volumes) );
				
				ui.link();
			}
		};

		struct CameraViewsUI {
			Application *application;
			
			AntTWBarUI::SimpleContainer ui;
			
			CameraViewsUI( Application *application ) : application( application ) {
				ui.setName( "Camera views" );

				auto cameraStatesView = AntTWBarUI::makeSharedVector( application->settings.views, NamedCameraStateView( application ) );
				ui.add( cameraStatesView );
				ui.add( AntTWBarUI::makeSharedButton( 
						"Add current view",
						[application, cameraStatesView] () {
							application->settings.views.push_back( aop::Settings::NamedCameraState() );
							application->settings.views.back().pullFrom( application->mainCamera );

							cameraStatesView->updateSize();
						}
					)
				);
				ui.add( AntTWBarUI::makeSharedButton( 
						"Clear all",
						[application, cameraStatesView] () {
							application->settings.views.clear();
							cameraStatesView->updateSize();
						}
					)
				);
				ui.link();
			}
		};

		/*struct ModelTypesUI {
			Application *application;

			AntTWBarUI
		};*/

		std::unique_ptr< MainUI > mainUI;
		std::unique_ptr< CameraViewsUI > cameraViewsUI;

		void initCamera() {
			mainCamera.perspectiveProjectionParameters.aspect = 640.0f / 480.0f;
			mainCamera.perspectiveProjectionParameters.FoV_y = 75.0f;
			mainCamera.perspectiveProjectionParameters.zNear = 0.05f;
			mainCamera.perspectiveProjectionParameters.zFar = 500.0f;

			mainCameraInputControl.init( make_nonallocated_shared( mainCamera ) );
		}

		void initUI() {
			mainUI.reset( new MainUI( this ) ) ;
			cameraViewsUI.reset( new CameraViewsUI( this ) );
		}
	};
}

void real_main() {
	sf::RenderWindow window( sf::VideoMode( 640, 480 ), "AOP", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
	glewInit();

	glutil::RegisterDebugOutput( glutil::STD_OUT );

	aop::Application application;

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	application.cameraView.renderContext.setDefault();

	const char *scenePath = "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene";
	application.world.init( scenePath );

	EventDispatcher eventDispatcher( "Root:" );
	
	EventSystem eventSystem;
	eventSystem.rootHandler = make_nonallocated_shared( eventDispatcher );
	eventSystem.exclusiveMode.window = make_nonallocated_shared( window );

/*	EventDispatcher verboseEventDispatcher( "sub" );
	eventDispatcher.addEventHandler( make_nonallocated_shared( verboseEventDispatcher ) );

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
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledInstanceIndexControl ) );*/

	AntTweakBarEventHandler antTweakBarEventHandler;
	antTweakBarEventHandler.init( window );
	// register anttweakbar first because it actually manages its own focus
	eventDispatcher.addEventHandler( make_nonallocated_shared( antTweakBarEventHandler ) );
	
	registerConsoleHelpAction( eventDispatcher );
	eventDispatcher.addEventHandler( make_nonallocated_shared( application.mainCameraInputControl ) );

	Editor editor;
	editor.world = &application.world;
	editor.view = &application.cameraView;
	editor.init();

	eventDispatcher.addEventHandler( make_nonallocated_shared( editor ) );

	DebugWindowManager debugWindowManager;

#if 0
	TextureVisualizationWindow optixWindow;
	optixWindow.init( "Optix Version" );
	optixWindow.texture = world.optixRenderer.debugTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( optixWindow ) );
#endif
#if 0
	TextureVisualizationWindow mergedTextureWindow;
	mergedTextureWindow.init( "merged object textures" );
	mergedTextureWindow.texture = world.sceneRenderer.mergedTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( mergedTextureWindow ) );
#endif

	sf::Text renderDuration;
	renderDuration.setPosition( 0, 0 );
	renderDuration.setCharacterSize( 10 );

	Instance testInstance;
	testInstance.modelId = 0;
	testInstance.transformation.setIdentity();
	//world.sceneRenderer.addInstance( testInstance );

	ProbeGenerator::initDirections();

#if 0
	CandidateFinder candidateFinder;
	candidateFinder.reserveIds(0);
	view.renderContext.disabledModelIndex = 0;
	DebugRender::DisplayList probeVisualization;
	{
		auto instanceIndices = world.sceneRenderer.getModelInstances( 0 );

		int totalCount = 0;

		for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
			boost::timer::auto_cpu_timer timer( "ProbeSampling, batch: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

			ProbeDataset dataset;
			std::vector<SGSInterface::Probe> transformedProbes;

			world.generateProbes( *instanceIndex, 0.25, dataset.probes, transformedProbes );

			std::cout << "sampling " << transformedProbes.size() << " probes in one batch:\n\t";
			world.optixRenderer.sampleProbes( transformedProbes, dataset.probeContexts, view.renderContext );

			candidateFinder.addDataset(0, std::move( dataset ) );

			totalCount += transformedProbes.size();

			/*probeVisualization.beginCompileAndAppend();
			visualizeProbes( 0.25, transformedProbes );
			probeVisualization.endCompile();*/
		}

		std::cout << "num probes: " << totalCount << "\n";

		candidateFinder.integrateDatasets();
	}
	view.renderContext.disabledModelIndex = -1;

	return;
#endif
	
	struct Editor_NamedVolumesView : Editor::Volumes {
		std::vector< aop::Settings::NamedTargetVolume > &volumes;

		Editor_NamedVolumesView( std::vector< aop::Settings::NamedTargetVolume > &volumes  ) : volumes( volumes ) {}

		int getCount() {
			return (int) volumes.size();
		}

		OBB *get( int index ) {
			return &volumes[ index ].volume;
		}
	};
	Editor_NamedVolumesView editor_namedVolumesView( application.settings.volumes );
	editor.volumes = &editor_namedVolumesView;
	
	{
		aop::Settings::NamedTargetVolume targetVolume;

		targetVolume.name = "test";
		targetVolume.volume.transformation.setIdentity();
		targetVolume.volume.size.setConstant( 3.0 );
		
		application.settings.volumes.push_back( targetVolume );
	}
	
	application.initCamera();
	application.initUI();

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
				application.mainCamera.perspectiveProjectionParameters.aspect = float( event.size.width ) / event.size.height;
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


			//probeVisualization.render();

			/*selectObjectsByModelID( world.sceneRenderer, view.renderContext.disabledModelIndex );
			glDisable( GL_DEPTH_TEST );
			selectionDR.render();
			glEnable( GL_DEPTH_TEST );*/

			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( application.cameraView.viewerContext.projectionView );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			application.cameraView.updateFromCamera( application.mainCamera );
			application.world.renderViewFrame( application.cameraView );
			editor.render();

			//world.renderOptixViewFrame( view );

			renderDuration.setString( renderTimer.format() );
			window.pushGLStates();
			window.resetGLStates();
			window.draw( renderDuration );
			window.popGLStates();
		}

		antTweakBarEventHandler.render();
		// End the current frame and display its contents on screen
		window.display();

		debugWindowManager.update();

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