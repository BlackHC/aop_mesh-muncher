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
#include "antTWBarUI.h"

#include "autoTimer.h"
#include "candidateFinderInterface.h"
#include "aopSettings.h"

#include "boost/range/algorithm_ext/push_back.hpp"
#include "boost/range/algorithm/unique.hpp"
#include "boost/range/algorithm/sort.hpp"
#include "boost/range/algorithm_ext/erase.hpp"

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
/*
namespace DebugOptions {
	bool visualizeInstanceProbes;	
}

namespace DebugInformation {

}

struct DebugUI {

};*/

const float probeResolution = 0.25;

void sampleInstances( SGSInterface::World *world, ProbeDatabase &candidateFinder, int modelIndex ) {
	AUTO_TIMER_FOR_FUNCTION();
	std::cerr << Indentation::get() << "sampling model " << modelIndex << "\n";

	RenderContext renderContext;
	renderContext.setDefault();
	//renderContext.disabledModelIndex = modelIndex;

	auto instanceIndices = world->sceneRenderer.getModelInstances( modelIndex );
	
	int totalCount = 0;

	for( int i = 0 ; i < instanceIndices.size() ; i++ ) {
		const int instanceIndex = instanceIndices[ i ];

		renderContext.disabledInstanceIndex = instanceIndex;

		ProbeDataset dataset;
		std::vector<SGSInterface::Probe> transformedProbes;
	
		world->generateProbes( instanceIndex, probeResolution, dataset.probes, transformedProbes );
	
		AUTO_TIMER_DEFAULT( boost::str( boost::format( "batch with %i probes for instance %i" ) % transformedProbes.size() % instanceIndex ) );
		
		world->optixRenderer.sampleProbes( transformedProbes, dataset.probeContexts, renderContext );
	
		candidateFinder.addDataset(modelIndex, std::move( dataset ) );
	
		totalCount += (int) transformedProbes.size();
	}
	
	std::cerr << Indentation::get() << "total sampled probes: " << totalCount << "\n";
}

void queryVolume( SGSInterface::World *world, ProbeDatabase &candidateFinder, const Obb &queryVolume ) {
	AUTO_TIMER_FOR_FUNCTION();

	RenderContext renderContext;
	renderContext.setDefault();

	ProbeDataset dataset;

	ProbeGenerator::generateQueryProbes( queryVolume, probeResolution, dataset.probes );
	
	{
		AUTO_TIMER_FOR_FUNCTION( "sampling scene");
		world->optixRenderer.sampleProbes( dataset.probes, dataset.probeContexts, renderContext );
	}
	
	{
		auto query = candidateFinder.createQuery();
		query->setQueryDataset( std::move( dataset ) );

		ProbeContextTolerance pct;
		pct.setDefault();
		query->setProbeContextTolerance( pct );

		query->execute();

		const auto &matchInfos = query->getCandidates();
		for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
			std::cout << Indentation::get() << matchInfo->id << ": " << matchInfo->numMatches << "\n";
		}
	}
}

// TODO: this should get its own file [9/30/2012 kirschan2]
EventSystem *EventHandler::eventSystem;

namespace aop {
	struct Application {
		sf::RenderWindow mainWindow;

		EventSystem eventSystem;
		EventDispatcher eventDispatcher;
		AntTweakBarEventHandler antTweakBarEventHandler;

		Camera mainCamera;
		CameraInputControl mainCameraInputControl;

		std::unique_ptr< SGSInterface::World > world;
		SGSInterface::View cameraView;

		Settings settings;

		ProbeDatabase candidateFinder;

		Application() {}

		struct MainUI {
			Application *application;
			AntTWBarUI::SimpleContainer ui;
			
			MainUI( Application *application ) : application( application ) {
				init();
			}

			void init() {
				ui.setName( "aop" );
				ui.add( AntTWBarUI::makeSharedButton( "Load settings", [this] { application->settings.load(); } ) );
				ui.add( AntTWBarUI::makeSharedButton( "Store settings", [this] { application->settings.store(); } ) );
				//ui.add( AntTWBarUI::makeSharedVector< NamedTargetVolumeView >( "Camera Views", application->settings.volumes) );
				ui.add( AntTWBarUI::makeSharedSeparator() );
				ui.add( AntTWBarUI::makeSharedButton( "Sample marked objects", [this] { 
					 const auto &modelIndices = application->modelTypesUI->markedModels;
					 for( auto modelIndex = modelIndices.begin() ; modelIndex != modelIndices.end() ; ++modelIndex ) {
					 	sampleInstances( application->world.get(), application->candidateFinder, *modelIndex );
					 }
					 application->candidateFinder.integrateDatasets();
				} ) );
				ui.add( AntTWBarUI::makeSharedSeparator() );
				ui.add( AntTWBarUI::makeSharedButton( "Query volume", [this] () {
					struct QueryVolumeVisitor : Editor::SelectionVisitor {
						Application *application;

						QueryVolumeVisitor( Application *application ) : application( application ) {}

						void visit() {
							std::cerr << "No volume selected!\n";
						}
						void visit( Editor::ObbSelection *selection ) {
							queryVolume( application->world.get(), application->candidateFinder, selection->getObb() );
						}
					};
					QueryVolumeVisitor( application ).dispatch( application->editorWrapper->editor.selection.get() );
				} ) );
				ui.add( AntTWBarUI::makeSharedSeparator() );
				ui.add( AntTWBarUI::makeSharedButton( "Load database", [this] { application->candidateFinder.loadCache( "database"); } ) );
				ui.add( AntTWBarUI::makeSharedButton( "Store database", [this] { application->candidateFinder.storeCache( "database"); } ) );
				ui.link();
			}
		};

		struct TargetVolumesUI {
			Application *application;

			AntTWBarUI::SimpleContainer ui;
			
			std::function<void()> update;

			struct NamedTargetVolumeView : AntTWBarUI::SimpleStructureFactory< aop::Settings::NamedTargetVolume, NamedTargetVolumeView > {
				TargetVolumesUI *targetVolumesUI;

				NamedTargetVolumeView( TargetVolumesUI *targetVolumesUI ) : targetVolumesUI( targetVolumesUI ) {}

				template< typename ElementAccessor >
				void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
					container->add( 
						AntTWBarUI::makeSharedVariable(
							"Name", 
							AntTWBarUI::makeMemberAccessor( accessor, &aop::Settings::NamedTargetVolume::name )
						)
					);
					container->add(
						AntTWBarUI::makeSharedButton(
							"Select",
							[] () { 
								std::cout << "select called\n";
							}
						)
					);
				}
			};

			TargetVolumesUI( Application *application ) : application( application ) {
				ui.setName( "Query volumes" );
				auto uiVector = AntTWBarUI::makeSharedVector( "Volumes", application->settings.volumes, NamedTargetVolumeView( this ) );
				
				ui.add( uiVector );
				ui.add( AntTWBarUI::makeSharedButton( "Add new",
						[this, uiVector] () {
							const auto &camera = this->application->mainCamera;
							aop::Settings::NamedTargetVolume volume;
							
							volume.volume.size = Eigen::Vector3f::Constant( 5.0 );
							volume.volume.transformation =
								Eigen::Translation3f( camera.getPosition() + 5.0 * camera.getDirection() ) *
								camera.getViewRotation().transpose()
								;

							this->application->settings.volumes.push_back( volume );

							uiVector->updateSize();
						}
					)
				);
				ui.link();

				update = [uiVector] () { uiVector->updateSize(); };
			}
		};

		struct CameraViewsUI {
			Application *application;
			
			AntTWBarUI::SimpleContainer ui;

			std::function< void() > update;

			struct NamedCameraStateView : AntTWBarUI::SimpleStructureFactory< aop::Settings::NamedCameraState, NamedCameraStateView >{
				Application *application;

				NamedCameraStateView( Application *application ) : application( application ) {}

				template< typename Accessor >
				void setup( AntTWBarUI::Container *container, Accessor &accessor ) const {
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

				update = [cameraStatesView] () {
					cameraStatesView->updateSize();
				};
			}
		};

		struct ModelTypesUI {
			Application *application;

			std::shared_ptr< AntTWBarUI::SimpleContainer > modelsUi;
			AntTWBarUI::SimpleContainer markedModelsUi;

			std::vector< std::string > beautifiedModelNames;
			std::vector< int > markedModels;

			std::function<void()> update;

			struct ModelNameView : AntTWBarUI::SimpleStructureFactory< std::string, ModelNameView >{
				ModelTypesUI *modelTypesUI;

				ModelNameView( ModelTypesUI *modelTypesUI ) : modelTypesUI( modelTypesUI ) {}
				
				template< typename ElementAccessor >
				void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
					container->add( AntTWBarUI::makeSharedButton( accessor.pull(), 
							[this, &accessor] () {
								modelTypesUI->toggleMarkedModel( accessor.elementIndex );
							}
						)
					);
				}
			};

			struct MarkedModelNameView : AntTWBarUI::SimpleStructureFactory< int, MarkedModelNameView > {
				ModelTypesUI *modelTypesUI;

				MarkedModelNameView( ModelTypesUI *modelTypesUI ) : modelTypesUI( modelTypesUI ) {}
				
				template< typename ElementAccessor >
				void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
					container->add( AntTWBarUI::makeSharedReadOnlyVariable(
							"Name",
							AntTWBarUI::makeExpressionAccessor<std::string>( [&] () -> std::string & { return modelTypesUI->beautifiedModelNames[ accessor.pull() ]; } )
						)
					);
				}
			};

			void beautifyModelNames() {
				for( auto filePath = application->world->scene.modelNames.begin() ; filePath != application->world->scene.modelNames.end() ; ++filePath ) {
					std::string filename = *filePath;
					auto offset = filename.find_last_of( "/\\" );
					if( offset != std::string::npos ) {
						filename = filename.substr( offset + 1 );
					}
					beautifiedModelNames.push_back( filename );
				}
			}

			void toggleMarkedModel( int index ) {
				auto found = boost::find( markedModels, index );
				if( found == markedModels.end() ) {
					markedModels.push_back( index );
				}
				else {
					markedModels.erase( found );
				}
				boost::sort( markedModels );
			}

			void replaceMarkedModels( const std::vector<int> modelIndices ) {
				markedModels = modelIndices;
				validateMarkedModels();
			}

			void validateMarkedModels() {
				boost::erase( markedModels, boost::unique< boost::return_found_end>( boost::sort( markedModels ) ) );
			}

			void appendMarkedModels( const std::vector<int> modelIndices ) {
				boost::push_back( markedModels, modelIndices );
				validateMarkedModels();
			}

			ModelTypesUI( Application *application ) : application( application ) {
				init();
			}

			struct ReplaceWithSelectionVisitor : Editor::SelectionVisitor {
				ModelTypesUI *modelTypesUI;

				ReplaceWithSelectionVisitor( ModelTypesUI *modelTypesUI ) : modelTypesUI( modelTypesUI ) {}

				void visit( Editor::SGSMultiModelSelection *selection ) {
					modelTypesUI->replaceMarkedModels( selection->modelIndices );
				}
			};

			struct AppendSelectionVisitor : Editor::SelectionVisitor {
				ModelTypesUI *modelTypesUI;

				AppendSelectionVisitor( ModelTypesUI *modelTypesUI ) : modelTypesUI( modelTypesUI ) {}

				void visit( Editor::SGSMultiModelSelection *selection ) {
					modelTypesUI->appendMarkedModels( selection->modelIndices );
				}
			};

			void init() {
				beautifyModelNames();

				struct MyConfig {
					enum { supportRemove = false };
				};
				modelsUi = std::make_shared< AntTWBarUI::Vector< ModelNameView, MyConfig > >( "All models", beautifiedModelNames, ModelNameView( this ), AntTWBarUI::CT_SEPARATOR );
				modelsUi->link();

				markedModelsUi.setName( "Marked models");
				auto markedModelsVector = AntTWBarUI::makeSharedVector( "Models", markedModels, MarkedModelNameView( this ), AntTWBarUI::CT_SEPARATOR );
				markedModelsUi.add( markedModelsVector );
				markedModelsUi.add( AntTWBarUI::makeSharedSeparator() );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "= {}", [this] () {
					markedModels.clear();
				} ) );				
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "= selection", [this] () {
					ReplaceWithSelectionVisitor( this ).dispatch( application->editorWrapper->editor.selection.get() );
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "+= selection", [this] () {
					AppendSelectionVisitor( this ).dispatch( application->editorWrapper->editor.selection.get() );
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedSeparator() );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "selection =", [this] () {
					application->editorWrapper->editor.selection = std::make_shared<Editor::SGSMultiModelSelection>( &application->editorWrapper->editor, markedModels );
				} ) );
				markedModelsUi.link();

				update = [markedModelsVector] () {
					markedModelsVector->updateSize();
				};
			}
		};

		struct EditorWrapper {
			Application *application;
			
			Editor editor;
			
			struct NamedVolumesView : Editor::Volumes {
				std::vector< aop::Settings::NamedTargetVolume > &volumes;

				NamedVolumesView( std::vector< aop::Settings::NamedTargetVolume > &volumes  ) : volumes( volumes ) {}

				int getCount() {
					return (int) volumes.size();
				}

				Obb *get( int index ) {
					if( index >= volumes.size() ) {
						return nullptr;
					}
					return &volumes[ index ].volume;
				}
			};
			NamedVolumesView namedVolumesView;

			EditorWrapper( Application *application ) : application( application ), namedVolumesView( application->settings.volumes ) {
				editor.world = application->world.get();
				editor.view = &application->cameraView;
				editor.volumes = &namedVolumesView;
				editor.init();

				application->eventDispatcher.addEventHandler( make_nonallocated_shared( editor ) );
			}
		};

		std::unique_ptr< MainUI > mainUI;
		std::unique_ptr< CameraViewsUI > cameraViewsUI;
		std::unique_ptr< TargetVolumesUI > targetVolumesUI;
		std::unique_ptr< ModelTypesUI > modelTypesUI;
		std::unique_ptr< EditorWrapper > editorWrapper;

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
			targetVolumesUI.reset( new TargetVolumesUI( this ) );
			modelTypesUI.reset( new ModelTypesUI( this ) );
		}

		void initMainWindow() {
			mainWindow.create( sf::VideoMode( 640, 480 ), "AOP", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
			glewInit();
			glutil::RegisterDebugOutput( glutil::STD_OUT );

			// Activate the window for OpenGL rendering
			mainWindow.setActive();

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);
			glClearDepth(1.f);
		}

		void initSGSInterface() {			
			cameraView.renderContext.setDefault();

			world.reset( new SGSInterface::World() );

			const char *scenePath = "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene";
			world->init( scenePath );
		}

		void initEventHandling() {
			eventDispatcher.name = "Input help:";
			eventSystem.rootHandler = make_nonallocated_shared( eventDispatcher );
			eventSystem.exclusiveMode.window = make_nonallocated_shared( mainWindow );

			registerConsoleHelpAction( eventDispatcher );
			eventDispatcher.addEventHandler( make_nonallocated_shared( mainCameraInputControl ) );
		}
	
		void init() {
			ProbeGenerator::initDirections();

			initMainWindow();
			initCamera();
			initEventHandling();

			initSGSInterface();

			candidateFinder.reserveIds( world->scene.modelNames.size() );

			editorWrapper.reset( new EditorWrapper( this ) );

			// register anttweakbar first because it actually manages its own focus
			antTweakBarEventHandler.init( mainWindow );
			eventDispatcher.addEventHandler( make_nonallocated_shared( antTweakBarEventHandler ) );

			initUI();
		}

		void updateUI() {
			targetVolumesUI->update();
			cameraViewsUI->update();
			modelTypesUI->update();
		}

		void eventLoop() {
			sf::Text renderDuration;
			renderDuration.setPosition( 0, 0 );
			renderDuration.setCharacterSize( 10 );

			sf::Clock frameClock, clock;

			while (true)
			{
				// Activate the window for OpenGL rendering
				mainWindow.setActive();

				// Event processing
				sf::Event event;
				while (mainWindow.pollEvent(event))
				{
					// Request for closing the window
					if (event.type == sf::Event::Closed)
						mainWindow.close();

					if( event.type == sf::Event::Resized ) {
						mainCamera.perspectiveProjectionParameters.aspect = float( event.size.width ) / event.size.height;
						glViewport( 0, 0, event.size.width, event.size.height );

						auto view = mainWindow.getView();
						view.reset( sf::FloatRect( 0.0f, 0.0f, (float) event.size.width, (float) event.size.height ) );
						mainWindow.setView( view );
					}

					eventSystem.processEvent( event );
				}

				updateUI();

				if( !mainWindow.isOpen() ) {
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
					glLoadMatrix( cameraView.viewerContext.projectionView );

					glMatrixMode( GL_MODELVIEW );
					glLoadIdentity();

					cameraView.updateFromCamera( mainCamera );

					world->renderViewFrame( cameraView );

					// render target volumes
					// render obbs
					DebugRender::begin();
					DebugRender::setColor( Eigen::Vector3f::Constant( 1.0 ) );
					for( auto namedObb = settings.volumes.begin() ; namedObb != settings.volumes.end() ; ++namedObb ) {
						DebugRender::setTransformation( namedObb->volume.transformation );
						DebugRender::drawBox( namedObb->volume.size );	
					}		
					DebugRender::end();

					// render editor entities first
					editorWrapper->editor.render();

					//world.renderOptixViewFrame( view );

					renderDuration.setString( renderTimer.format() );

					mainWindow.pushGLStates();
					mainWindow.resetGLStates();
					mainWindow.draw( renderDuration );
					mainWindow.popGLStates();
				}

				antTweakBarEventHandler.render();
				// End the current frame and display its contents on screen
				mainWindow.display();

				//debugWindowManager.update();
			}
		}
	};
}

void real_main() {
	aop::Application application;

	application.init();

	// The main loop - ends as soon as the window is closed
	

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

	//DebugWindowManager debugWindowManager;

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
	
	{
		aop::Settings::NamedTargetVolume targetVolume;

		targetVolume.name = "test";
		targetVolume.volume.transformation.setIdentity();
		targetVolume.volume.size.setConstant( 3.0 );
		
		application.settings.volumes.push_back( targetVolume );
	}

	application.eventLoop();
};

void main() {
	try {
		aop::Application application;

		application.init();

		application.eventLoop();
	}
	catch( std::exception &e) {
		std::cout << e.what() << "\n";
	}
}