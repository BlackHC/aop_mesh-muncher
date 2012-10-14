#include "windows.h"
#include <windowsx.h>
#include <shlobj.h>
#include <strsafe.h>


//
// 


#include "aopApplication.h"

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
#include "probeDatabase.h"
#include "aopSettings.h"

#include "boost/range/algorithm_ext/push_back.hpp"
#include "boost/range/algorithm/unique.hpp"
#include "boost/range/algorithm/sort.hpp"
#include "boost/range/algorithm_ext/erase.hpp"

#include "contextHelper.h"
#include "boost/range/algorithm/copy.hpp"
#include "boost/range/adaptor/transformed.hpp"

#include "viewportContext.h"
#include "widgets.h"
#include "modelButtonWidget.h"

#include "logger.h"
#include <progressTracker.h>

#include "neighborhoodDatabase.h"
#include "aopCandidateSidebarUI.h"
#include "aopQueryVolumesUI.h"
#include "aopCameraViewsUI.h"
#include "aopModelTypesUI.h"
#include "aopTimedLog.h"

enum GridVisualizationMode {
	GVM_POSITION,
	GVM_HITS,
	GVM_NORMAL,
	GVM_MAX
};

void visualizeColorGrid( const VoxelizedModel::Voxels &grid, GridVisualizationMode gvm = GVM_POSITION ) {
	const float size = grid.getMapping().getResolution();
	
	DebugRender::begin();
	for( auto iterator = grid.getIterator() ; iterator.hasMore() ; ++iterator ) {
		const auto &normalHit = grid[ *iterator ];

		if( normalHit.numSamples != 0 ) {
			DebugRender::setPosition( grid.getMapping().getPosition( iterator.getIndex3() ) );

			Eigen::Vector3f positionColor = iterator.getIndex3().cast<float>().cwiseQuotient( grid.getMapping().getSize().cast<float>() );

			switch( gvm ) {
			case GVM_POSITION:
				DebugRender::setColor( positionColor );
				break;
			case GVM_HITS:
				DebugRender::setColor( Vector3f::UnitY() * (0.5 + normalHit.numSamples / 128.0) );
				break;
			case GVM_NORMAL:
				glColor3ubv( &normalHit.nx );
				break;
			}
			
			DebugRender::drawBox( Vector3f::Constant( size ), false );
		}
	}
	DebugRender::end();
}

const float neighborhoodMaxDistance = 100.0;
// TODO XXX [10/11/2012 kirschan2]

std::weak_ptr<AntTweakBarEventHandler::GlobalScope> AntTweakBarEventHandler::globalScope;

void visualizeProbes( float resolution, const std::vector< SGSInterface::Probe > &probes );

// TODO: reorder the parameters in some consistent way! [10/10/2012 kirschan2]
void sampleAllNeighbors( float maxDistance, NeighborhoodDatabase &database, SGSInterface::World &world ) {
	AUTO_TIMER_FOR_FUNCTION();

	const int numInstances = world.sceneRenderer.getNumInstances();

	for( int instanceIndex = 0 ; instanceIndex < numInstances ; instanceIndex++ ) {
		auto queryResults = world.sceneGrid.query( 
			-1,
			instanceIndex, 
			world.sceneRenderer.getInstanceTransformation( instanceIndex ).translation(),
			maxDistance
		);

		const int modelIndex = world.sceneRenderer.getModelIndex( instanceIndex );

		database.getEntryById( modelIndex ).addInstance( std::move( queryResults ) );
	}
}

NeighborhoodDatabase::Query::Results queryVolumeNeighbors( SGSInterface::World *world, NeighborhoodDatabase &database, const Vector3f &position, float maxDistance, float tolerance ) {
	AUTO_TIMER_FOR_FUNCTION();

	auto sceneQueryResults = world->sceneGrid.query( -1, -1, position, maxDistance );

	NeighborhoodDatabase::Query query( database, tolerance, maxDistance, std::move( sceneQueryResults ) );
	auto results = query.execute();

	for( auto result = results.begin() ; result != results.end() ; ++result ) {
		log( boost::format( "%i: %f" ) % result->second % result->first );
	}

	return results;
}

//////////////////////////////////////////////////////////////////////////
// V2
// 

void sampleAllNeighborsV2( float maxDistance, NeighborhoodDatabaseV2 &database, SGSInterface::World &world ) {
	AUTO_TIMER_FOR_FUNCTION();

	database.numIds = world.scene.models.size();

	const int numInstances = world.sceneRenderer.getNumInstances();

	for( int instanceIndex = 0 ; instanceIndex < numInstances ; instanceIndex++ ) {
		auto queryResults = world.sceneGrid.query( 
			-1,
			instanceIndex, 
			world.sceneRenderer.getInstanceTransformation( instanceIndex ).translation(),
			maxDistance
		);

		const int modelIndex = world.sceneRenderer.getModelIndex( instanceIndex );

		database.addInstance( modelIndex, std::move( queryResults ) );
	}
}

NeighborhoodDatabaseV2::Query::Results queryVolumeNeighborsV2( SGSInterface::World *world, NeighborhoodDatabaseV2 &database, const Vector3f &position, float maxDistance, float tolerance ) {
	AUTO_TIMER_FOR_FUNCTION();

	auto sceneQueryResults = world->sceneGrid.query( -1, -1, position, maxDistance );

	NeighborhoodDatabaseV2::Query query( database, tolerance, std::move( sceneQueryResults ) );
	auto results = query.execute();

	for( auto result = results.begin() ; result != results.end() ; ++result ) {
		log( boost::format( "%i: %f" ) % result->second % result->first );
	}

	return results;
}
//////////////////////////////////////////////////////////////////////////

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

struct DebugUI;

struct IDebugObject {
	struct Tag {};
	typedef std::shared_ptr< IDebugObject > SPtr;

	virtual void link( DebugUI *debugUI, Tag = Tag() ) = 0;
	virtual AntTWBarUI::Element::SPtr getUI( Tag = Tag() ) = 0;

	// called after the rest of the scene has been rendered
	virtual void renderScene( Tag = Tag() ) = 0;
};

struct DebugUI {
	struct Tag {};

	DebugUI() : container( "Debug" ), debugObjectsContainer( "Objects" ) {
		onAddStaticUI( container );
		container.add( make_nonallocated_shared( debugObjectsContainer ) );

		container.link();
	}

	void add( IDebugObject::SPtr debugObject ) {
		debugObject->link( this );
		debugObjects.push_back( debugObject );

		debugObjectsContainer.add( debugObject->getUI() );
	}

	void remove( IDebugObject *debugObject ) {
		debugObjectsContainer.remove( debugObject->getUI().get() );

		for( auto myDebugObject = debugObjects.begin() ; myDebugObject != debugObjects.end() ; ++myDebugObject ) {
			if( myDebugObject->get() == debugObject ) {
				debugObjects.erase( myDebugObject );
				return;
			}
		}
	}

	void refresh() {
		container.refresh();
	}

private:
	// order is important here because debugObjectsContainer might contain stack pointers to debugObjects
	std::vector< IDebugObject::SPtr > debugObjects;
	AntTWBarUI::SimpleContainer debugObjectsContainer;
	AntTWBarUI::SimpleContainer container;

	virtual void onAddStaticUI( AntTWBarUI::SimpleContainer &container, Tag = Tag() ) {}
};

namespace aop {
namespace DebugObjects {
	struct OptixView : IDebugObject {
		Application *application;

		DebugUI *debugUI;

		AntTWBarUI::SimpleContainer container;

		OptixView( Application *application ) : container( AntTWBarUI::CT_EMBEDDED ), application( application ) {
			init();
		}

		void init() {
			container.add( AntTWBarUI::makeSharedButton( 
				"Create optix view window",
				[&] () {
					auto window = std::make_shared< TextureVisualizationWindow >();
					window->init( "Optix View" );
					window->texture = application->world->optixRenderer.debugTexture;
					application->debugWindowManager.add( window );

					application->renderOptixView = true;					
				}
			) );
			container.add( AntTWBarUI::makeSharedVariable( 
				"Update optix view",
				AntTWBarUI::makeReferenceAccessor( application->renderOptixView )
			) );
		}

		// TODO: maybe make this just a normal member of IDebugObject?
		virtual void link( DebugUI *debugUI, Tag ) {
			this->debugUI = debugUI;
		}

		virtual AntTWBarUI::Element::SPtr getUI( Tag ) {
			return make_nonallocated_shared( container );	
		}

		virtual void renderScene( Tag ) {
		}
	};

	struct ModelDatabase : IDebugObject {
		Application *application;

		DebugUI *debugUI;

		AntTWBarUI::SimpleContainer container;

		ModelDatabase( Application *application ) : container( AntTWBarUI::CT_GROUP ), application( application ) {
			init();
		}

		void init() {
			container.setName( "Model Database" );
			for( int modelIndex = 0 ; modelIndex < application->modelDatabase.informationById.size() ; modelIndex++ ) {
				container.add( AntTWBarUI::makeSharedButton( 
					application->modelDatabase.informationById[ modelIndex ].shortName,
					[&, modelIndex] () {
						auto window = std::make_shared< MultiDisplayListVisualizationWindow >();
						
						// render the object
						{
							auto &modelVisualization = window->visualizations[0];
							modelVisualization.name = "model";
							window->makeVisible( 0 );
							modelVisualization.displayList.create();
							modelVisualization.displayList.begin();
							application->world->sceneRenderer.renderModel( Vector3f::Zero(), modelIndex );
							modelVisualization.displayList.end();
						}
						// render the samples using position colors
						{
							auto &voxelVisualization = window->visualizations[1];
							voxelVisualization.name = "voxels (colored using positions)";
							voxelVisualization.displayList.create();
							voxelVisualization.displayList.begin();
							visualizeColorGrid( application->modelDatabase.informationById[ modelIndex ].voxels, GVM_POSITION );
							voxelVisualization.displayList.end();
						}
						// render the samples using hits
						{
							auto &voxelVisualization = window->visualizations[2];
							voxelVisualization.name = "voxels (colored using overdraw)";
							voxelVisualization.displayList.create();
							voxelVisualization.displayList.begin();
							visualizeColorGrid( application->modelDatabase.informationById[ modelIndex ].voxels, GVM_HITS );
							voxelVisualization.displayList.end();
						}
						// render the samples using hits
						{
							auto &voxelVisualization = window->visualizations[3];
							voxelVisualization.name = "voxels (colored using normals)";
							voxelVisualization.displayList.create();
							voxelVisualization.displayList.begin();
							visualizeColorGrid( application->modelDatabase.informationById[ modelIndex ].voxels, GVM_NORMAL );
							voxelVisualization.displayList.end();
						}
						// render the samples using samples
						{
							auto &voxelVisualization = window->visualizations[4];
							voxelVisualization.name = "probes";
							voxelVisualization.displayList.create();
							voxelVisualization.displayList.begin();
							visualizeProbes( 
								application->modelDatabase.informationById[ modelIndex ].voxels.getMapping().getResolution(),
								application->modelDatabase.informationById[ modelIndex ].probes
							);
							voxelVisualization.displayList.end();
						}

						window->keyLogics[1].disableMask = window->keyLogics[2].disableMask = window->keyLogics[3].disableMask = 2+4+8;


						window->init( application->modelDatabase.informationById[ modelIndex ].name );

						application->debugWindowManager.add( window );
					}
				) );
			}
		}

		// TODO: maybe make this just a normal member of IDebugObject?
		virtual void link( DebugUI *debugUI, Tag ) {
			this->debugUI = debugUI;
		}

		virtual AntTWBarUI::Element::SPtr getUI( Tag ) {
			return make_nonallocated_shared( container );
		}

		virtual void renderScene( Tag ) {
		}
	};
}
}

namespace aop {
	struct ModelDatabaseUI {
		Application *application;

		AntTWBarUI::SimpleContainer ui;

		ModelDatabaseUI( Application *application ) : application( application ) {
			init();
		}

		void init() {
			ui.setName( "Model Database" );
			ui.add( AntTWBarUI::makeSharedButton( "Load", [&] () { application->modelDatabase.load( "modelDatabase" ); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store", [&] () { application->modelDatabase.store( "modelDatabase" ); } ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Sample models", [&] { 
				application->startLongOperation();
				application->ModelDatabase_sampleAll(); 
				application->endLongOperation();
			} ) );
			ui.link();
		}

		void refresh() {
			ui.refresh();
		}
	};
}

const float probeResolution = 0.25;

namespace aop {
	struct Application::NamedVolumesEditorView : Editor::Volumes {
		std::vector< aop::Settings::NamedTargetVolume > &volumes;

		NamedVolumesEditorView( std::vector< aop::Settings::NamedTargetVolume > &volumes  ) : volumes( volumes ) {}

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

	struct Application::MainUI {
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
				application->startLongOperation();

				const auto &modelIndices = application->modelTypesUI->markedModels;

				ProgressTracker::Context progressTracker( modelIndices.size() + 1 );
				for( auto modelIndex = modelIndices.begin() ; modelIndex != modelIndices.end() ; ++modelIndex ) {
					application->sampleInstances( *modelIndex );
					progressTracker.markFinished();
				}
				
				application->probeDatabase.integrateDatasets();
				progressTracker.markFinished();

				application->endLongOperation();
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedVariable( "Query tolerance", AntTWBarUI::makeReferenceAccessor( application->settings.neighborhoodQueryTolerance ) ) );
			ui.add( AntTWBarUI::makeSharedButton( "Query volume", [this] () {
				struct QueryVolumeVisitor : Editor::SelectionVisitor {
					Application *application;

					QueryVolumeVisitor( Application *application ) : application( application ) {}

					void visit() {
						std::cerr << "No volume selected!\n";
					}
					void visit( Editor::ObbSelection *selection ) {
						application->startLongOperation();
						auto matchInfos = application->queryVolume( selection->getObb() );
						application->endLongOperation();

						typedef ProbeDatabase::Query::MatchInfo MatchInfo;
						boost::sort( 
							matchInfos,
							[] (const MatchInfo &a, MatchInfo &b ) {
								return a.score > b.score;
							}
						);

						std::vector<int> modelIndices;
						for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
							modelIndices.push_back( matchInfo->id );	
						}

						application->candidateSidebarUI->clear();
						application->candidateSidebarUI->addModels( modelIndices, selection->getObb().transformation.translation() );
					}
				};
				QueryVolumeVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Query volume (full)", [this] () {
				struct QueryVolumeVisitor : Editor::SelectionVisitor {
					Application *application;

					QueryVolumeVisitor( Application *application ) : application( application ) {}

					void visit() {
						std::cerr << "No volume selected!\n";
					}
					void visit( Editor::ObbSelection *selection ) {
						application->startLongOperation();
						auto matchInfos = application->fullQueryVolume( selection->getObb() );
						application->endLongOperation();

						typedef ProbeDatabase::FullQuery::MatchInfo MatchInfo;
						boost::sort( 
							matchInfos,
							[] (const MatchInfo &a, MatchInfo &b ) {
								return a.matches.size() > b.matches.size();
							}
						);

						std::vector<int> modelIndices;
						for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
							modelIndices.push_back( matchInfo->id );
						}

						application->candidateSidebarUI->clear();
						application->candidateSidebarUI->addModels( modelIndices, selection->getObb().transformation.translation() );
					}
				};
				QueryVolumeVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Query neighbors", [this] () {
				struct QueryNeighborsVisitor : Editor::SelectionVisitor {
					Application *application;

					QueryNeighborsVisitor( Application *application ) : application( application ) {}

					void visit() {
						logError( "No volume selected!\n" );
					}
					void visit( Editor::ObbSelection *selection ) {
						application->startLongOperation();
						auto queryResults = queryVolumeNeighbors(
							application->world.get(),
							application->neighborDatabase,
							selection->getObb().transformation.translation(),
							neighborhoodMaxDistance,
							application->settings.neighborhoodQueryTolerance
						);
						application->endLongOperation();

						std::vector<int> modelIndices;
						for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
							modelIndices.push_back( queryResult->second );	
						}

						application->candidateSidebarUI->clear();
						application->candidateSidebarUI->addModels( modelIndices, selection->getObb().transformation.translation() );
					}
				};
				QueryNeighborsVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Query neighbors V2", [this] () {
				struct QueryNeighborsVisitor : Editor::SelectionVisitor {
					Application *application;

					QueryNeighborsVisitor( Application *application ) : application( application ) {}

					void visit() {
						logError( "No volume selected!\n" );
					}
					void visit( Editor::ObbSelection *selection ) {
						application->startLongOperation();
						auto queryResults = queryVolumeNeighborsV2(
							application->world.get(),
							application->neighborDatabaseV2,
							selection->getObb().transformation.translation(),
							neighborhoodMaxDistance, 
							application->settings.neighborhoodQueryTolerance
						);
						application->endLongOperation();

						std::vector<int> modelIndices;
						for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
							modelIndices.push_back( queryResult->second );	
						}

						application->candidateSidebarUI->clear();
						application->candidateSidebarUI->addModels( modelIndices, selection->getObb().transformation.translation() );
					}
				};
				QueryNeighborsVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Load database", [this] { application->probeDatabase.loadCache( "database"); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Reset database", [this] { application->probeDatabase.reset(); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store database", [this] { application->probeDatabase.storeCache( "database"); } ) );
			ui.link();
		}

		void update() {
			ui.refresh();
		}
	};

	void Application::initCamera() {
		mainCamera.perspectiveProjectionParameters.aspect = 640.0f / 480.0f;
		mainCamera.perspectiveProjectionParameters.FoV_y = 75.0f;
		mainCamera.perspectiveProjectionParameters.zNear = 0.05f;
		mainCamera.perspectiveProjectionParameters.zFar = 500.0f;

		mainCameraInputControl.init( make_nonallocated_shared( mainCamera ) );
	}

	void Application::initUI() {
		mainUI.reset( new MainUI( this ) ) ;
		cameraViewsUI.reset( new CameraViewsUI( this ) );
		targetVolumesUI.reset( new TargetVolumesUI( this ) );
		modelTypesUI.reset( new ModelTypesUI( this ) );
	
		modelDatabaseUI = std::make_shared< ModelDatabaseUI >( this );

		candidateSidebarUI = createCandidateSidebarUI( this );
		debugUI = std::make_shared< DebugUI >();

		debugUI->add( std::make_shared< DebugObjects::OptixView >( this ) );
	}

	void Application::initMainWindow() {
		mainWindow.create( sf::VideoMode( 640, 480 ), "AOP", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
		glewInit();
		glutil::RegisterDebugOutput( glutil::STD_OUT );

		// Activate the window for OpenGL rendering
		mainWindow.setActive();

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClearDepth(1.f);
	}

	void Application::initSGSInterface() {
		cameraView.renderContext.setDefault();

		world.reset( new SGSInterface::World() );

		const char *scenePath = "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene";
		world->init( scenePath );
	}

	// this just fills the model database with whatever data we can quickly extract from the scene
	void Application::ModelDatabase_init() {
		const auto &models = world->scene.models;
		const int numModels = models.size();

		ProgressTracker::Context progressTracker( numModels );

		for( int modelId = 0 ; modelId < numModels ; modelId++ ) {
			const auto bbox = world->sceneRenderer.getModelBoundingBox( modelId );

			ModelDatabase::IdInformation idInformation;

			{
				std::string filename = idInformation.name = world->scene.modelNames[ modelId ];
				auto offset = filename.find_last_of( "/\\" );
				if( offset != std::string::npos ) {
					filename = filename.substr( offset + 1 );
				}
				idInformation.shortName = filename;
			}

			const Vector3f sizes = bbox.sizes();
			idInformation.diagonalLength = sizes.norm();
			// sucks for IND## idInformation.area = sizes.prod() * sizes.cwiseInverse().sum() * 2;
			idInformation.area =
				2 * sizes[0] * sizes[1] +
				2 * sizes[1] * sizes[2] +
				2 * sizes[0] * sizes[2]
			;
			idInformation.volume = sizes.prod();

			modelDatabase.informationById.emplace_back( std::move( idInformation ) );
		}

		debugUI->add( std::make_shared< DebugObjects::ModelDatabase >( this ) );
	}
	
	int Application::ModelDatabase_sampleModel( int modelId, float resolution ) {
		AUTO_TIMER( boost::format( "model %i") % modelId );

		const auto bbox = world->sceneRenderer.getModelBoundingBox( modelId );

		ModelDatabase::IdInformation & idInformation = modelDatabase.informationById[ modelId ];

		idInformation.voxelResolution = resolution;
			
		const auto &voxels = idInformation.voxels = AUTO_TIME( world->sceneRenderer.voxelizeModel( modelId, resolution ), "voxelizing");

		auto &probes = idInformation.probes;
		probes.reserve( voxels.getMapping().count * 13 );
		probes.clear();

		int numNonEmpty = 0;
		AUTO_TIMER_BLOCK( "creating probes" ) {
			for( auto iter = voxels.getIterator() ; iter.hasMore() ; ++iter ) {
				const auto &sample = voxels[ *iter ];

				if( sample.numSamples > 0 ) {
					numNonEmpty++;

					const Vector3f position = voxels.getMapping().getPosition( iter.getIndex3() );

					/*const Vector3f normal(
						sample.nx / 255.0 * 2 - 1.0,
						sample.ny / 255.0 * 2 - 1.0,
						sample.nz / 255.0 * 2 - 1.0
					);*/

					// we dont use the weighted and averaged normal for now
					const Vector3f normal = (position - bbox.center()).normalized();

					ProbeGenerator::appendProbesFromSample( position, normal, probes );
				}
			}
		}

		probes.shrink_to_fit();

		const int count = voxels.getMapping().count;

		log( boost::format( 
			"Ratio %f = %i / %i (%i probes)" ) 
			% (float( numNonEmpty ) / count) 
			% numNonEmpty 
			% voxels.getMapping().count 
			% probes.size()
		);

		return numNonEmpty;
	}

	// this samples/voxelizes all models in the scene and create probes
	void Application::ModelDatabase_sampleAll() {
		AUTO_TIMER();

		const auto &models = world->scene.models;
		const int numModels = models.size();

		ProgressTracker::Context progressTracker( numModels );

		const float resolution = 0.25;

		int totalNonEmpty = 0;
		int totalCounts = 0;
		int totalProbes = 0;

		for( int modelId = 0 ; modelId < numModels ; modelId++ ) {
			const int numNonEmpty = ModelDatabase_sampleModel( modelId, resolution );

			ModelDatabase::IdInformation & idInformation = modelDatabase.informationById[ modelId ];

			totalCounts += idInformation.voxels.getMapping().count;
			totalNonEmpty += numNonEmpty;
			totalProbes += idInformation.probes.size();

			progressTracker.markFinished();
		}

		log( boost::format( 
			"Total ratio %f = %i / %i (%i probes in total)" ) 
			% (float( totalNonEmpty ) / totalCounts) 
			% totalNonEmpty 
			% totalCounts
			% totalProbes
		);
	}

	void Application::sampleInstances( int modelIndex ) {
		AUTO_TIMER_FOR_FUNCTION();
		log( boost::format( "sampling model %i" ) % modelIndex );

		RenderContext renderContext;
		renderContext.setDefault();
		//renderContext.disabledModelIndex = modelIndex;

		auto instanceIndices = world->sceneRenderer.getModelInstances( modelIndex );

		ProgressTracker::Context progressTracker( instanceIndices.size() * 3 );

		int totalCount = 0;

		for( int i = 0 ; i < instanceIndices.size() ; i++ ) {
			const int instanceIndex = instanceIndices[ i ];
			
			const auto &probes = modelDatabase.getProbes( modelIndex, probeResolution );

			std::vector<SGSInterface::Probe> transformedProbes;
			{
				const auto &transformation = world->sceneRenderer.getInstanceTransformation( instanceIndex );
				ProbeGenerator::transformProbes( probes, transformation, transformedProbes );
			}

			RawProbeDataset rawDataset;

			progressTracker.markFinished();

			AUTO_TIMER_BLOCK( boost::str( boost::format( "sampling probe batch with %i probes for instance %i" ) % probes.size() % instanceIndex ) ) {
				renderContext.disabledInstanceIndex = instanceIndex;
				world->optixRenderer.sampleProbes( transformedProbes, rawDataset, renderContext );
			}
			progressTracker.markFinished();

			probeDatabase.addDataset(modelIndex, probes, SortedProbeDataset( rawDataset ) );
			progressTracker.markFinished();

			totalCount += (int) transformedProbes.size();
		}

		log( boost::format( "total sampled probes: %i" ) % totalCount );
	}

	ProbeDatabase::Query::MatchInfos Application::queryVolume( const Obb &queryVolume ) {
		ProgressTracker::Context progressTracker(4);

		AUTO_TIMER_FOR_FUNCTION();

		RenderContext renderContext;
		renderContext.setDefault();

		std::vector<OptixProgramInterface::Probe> probes;

		ProbeGenerator::generateQueryProbes( queryVolume, probeResolution, probes );

		progressTracker.markFinished();

		RawProbeDataset rawDataset;
		AUTO_TIMER_BLOCK( "sampling scene") {
			world->optixRenderer.sampleProbes( probes, rawDataset, renderContext );
		}
		progressTracker.markFinished();

		ProbeDatabase::Query query( probeDatabase );
		{
			query.setQueryDataset( std::move( rawDataset ) );

			ProbeContextTolerance pct;
			pct.setDefault();
			query.setProbeContextTolerance( pct );

			query.execute();
		}
		progressTracker.markFinished();

		const auto &matchInfos = query.getCandidates();
		for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
			log( 
				boost::format( 
					"%i:\tnumMatches %f\n"
					"\tdbMatchPercentage %f\n"
					"\tqueryMatchPercentage %f\n"
					"\tscore %f\n"
				) 
				% matchInfo->id 
				% matchInfo->numMatches
				% matchInfo->probeMatchPercentage
				% matchInfo->queryMatchPercentage
				% matchInfo->score
			);
		}
		progressTracker.markFinished();

		return matchInfos;
	}

	ProbeDatabase::FullQuery::MatchInfos Application::fullQueryVolume( const Obb &queryVolume ) {
		ProgressTracker::Context progressTracker(4);

		AUTO_TIMER_FOR_FUNCTION();

		RenderContext renderContext;
		renderContext.setDefault();

		std::vector<OptixProgramInterface::Probe> probes;
		ProbeGenerator::generateQueryProbes( queryVolume, probeResolution, probes );

		progressTracker.markFinished();

		RawProbeDataset rawDataset;
		{
			AUTO_TIMER_FOR_FUNCTION( "sampling scene");
			world->optixRenderer.sampleProbes( probes, rawDataset, renderContext );
		}
		progressTracker.markFinished();

		ProbeDatabase::FullQuery query( probeDatabase );
		query.setQueryDataset( std::move( rawDataset ) );

		ProbeContextTolerance pct;
		pct.setDefault();
		query.setProbeContextTolerance( pct );

		auto matchInfos = query.execute();
		progressTracker.markFinished();

		for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
			log( 
				boost::format( 
					"%i:\tnumMatches %f\n"
					"\tdbMatchPercentage %f\n"
					"\tqueryMatchPercentage %f\n"
					"\tscore %f\n"
				) 
				% matchInfo->id 
				% matchInfo->matches.size()
			);
		}
		progressTracker.markFinished();

		return std::move( matchInfos );
	}

	// TODO: rearrange the functions in this file or split it up into several files or several classes [10/14/2012 kirschan2]

	void Application::initEventHandling() {
		eventDispatcher.name = "Input help:";
		eventSystem.setRootHandler( make_nonallocated_shared( eventDispatcher ) );
		eventSystem.exclusiveMode.window = make_nonallocated_shared( mainWindow );

		registerConsoleHelpAction( eventDispatcher );
		eventDispatcher.addEventHandler( make_nonallocated_shared( mainCameraInputControl ) );
	}

	void Application::initEditor() {
		editor.world = world.get();
		editor.view = &cameraView;
		editor.volumes = namedVolumesEditorView.get();
		editor.init();

		eventDispatcher.addEventHandler( make_nonallocated_shared( editor ) );
	}

	void Application::init() {
		Log::Sinks::addStdio();

		timedLog.reset( new TimedLog( this ) );

		ProbeGenerator::initDirections();

		initMainWindow();
		initCamera();
		initEventHandling();

		initSGSInterface();

		// TODO: fix parameter order [10/10/2012 kirschan2]
		//sampleAllNeighbors( neighborhoodMaxDistance, neighborDatabase, *world );
		//sampleAllNeighborsV2( neighborhoodMaxDistance, neighborDatabaseV2, *world );

		probeDatabase.reserveIds( world->scene.modelNames.size() );

		namedVolumesEditorView.reset( new NamedVolumesEditorView( settings.volumes ) );

		initEditor();

		eventDispatcher.addEventHandler( make_nonallocated_shared( widgetRoot ) );

		// register anttweakbar first because it actually manages its own focus
		antTweakBarEventHandler.init( mainWindow );
		eventDispatcher.addEventHandler( make_nonallocated_shared( antTweakBarEventHandler ) );

		initUI();

		ModelDatabase_init();
		neighborDatabaseV2.modelDatabase = &modelDatabase;

		// load settings
		settings.load();

		if( !settings.views.empty() ) {
			settings.views.front().pushTo( mainCamera );
		}

		if( !settings.volumes.empty() ) {
			// TODO: add select wrappers to editorWrapper or editor [10/3/2012 kirschan2]
			editor.selectObb( 0 );
		}
	}

	void Application::updateUI() {
		targetVolumesUI->update();
		cameraViewsUI->update();
		modelTypesUI->update();
		mainUI->update();
		// TODO: settle on refresh or update [10/12/2012 kirschan2]
		debugUI->refresh();

		modelDatabaseUI->refresh();
	}

	void Application::eventLoop() {
		sf::Text renderDuration;
		renderDuration.setCharacterSize( 12 );

		KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { world->sceneRenderer.reloadShaders(); } );
		eventDispatcher.addEventHandler( make_nonallocated_shared( reloadShadersAction ) );

		while (true)
		{
			debugWindowManager.update();

			// Activate the window for OpenGL rendering
			mainWindow.setActive();

			const sf::Vector2i windowSize( mainWindow.getSize() );
			ViewportContext viewportContext( windowSize.x, windowSize.y );

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

			if( !mainWindow.isOpen() ) {
				break;
			}

			// we set the main window again because we might have created another window in-between
			mainWindow.setActive();

			eventSystem.update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );
				
			updateUI();

			timedLog->updateTime( clock.getElapsedTime().asSeconds() );
			timedLog->updateText();

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
				editor.render();

				if( renderOptixView ) {
					world->renderOptixViewFrame( cameraView );
				}

				// render widgets
				glMatrixMode( GL_PROJECTION );
				glLoadIdentity();

				glLoadMatrix( Eigen::Scaling<float>( 1.0f, -1.0f, 1.0f) * Eigen::Translation3f( Vector3f( -1.0, -1.0, 0.0 ) ) * Eigen::Scaling<float>( 2.0f / windowSize.x, 2.0f / windowSize.y, 1.0f ) );

				widgetRoot.transformChain.localTransform = Eigen::Scaling<float>( windowSize.x, windowSize.y, 1.0 );

				glMatrixMode( GL_MODELVIEW );
				glLoadIdentity();

				glDisable( GL_DEPTH_TEST );
				glClear( GL_DEPTH_BUFFER_BIT );

				widgetRoot.onRender();

				glEnable( GL_DEPTH_TEST );

				renderDuration.setString( renderTimer.format() );
				
				mainWindow.pushGLStates();
				mainWindow.resetGLStates();

				{
					const auto height = renderDuration.getLocalBounds().height;
					renderDuration.setPosition( 0.0, windowSize.y - height );
					mainWindow.draw( renderDuration );
				}
				
				timedLog->renderAsNotifications();

				mainWindow.popGLStates();
			}

			antTweakBarEventHandler.render();
			// End the current frame and display its contents on screen
			mainWindow.display();

			//debugWindowManager.update();
		}
	}

	// TODO: fix this variable hack [10/14/2012 kirschan2]
	static float lastUpdateTime;
	void Application::updateProgress() {
		const float minDuration = 1.0/25.0f; // target fps 
		const float currentTime = clock.getElapsedTime().asSeconds();
		if( currentTime - lastUpdateTime < minDuration ) {
			return;
		}
		lastUpdateTime = currentTime;

		const sf::Vector2i windowSize( mainWindow.getSize() );

		mainWindow.pushGLStates();
		mainWindow.resetGLStates();
		glClearColor( 0.2, 0.2, 0.2, 1.0 );
		mainWindow.clear();
		glClearColor( 0.0, 0.0, 0.0, 0.0 );
	
		timedLog->updateTime( clock.getElapsedTime().asSeconds() );
		timedLog->updateText();
		timedLog->renderAsLog();

		sf::RectangleShape progressBar;
		progressBar.setPosition( 0.0, windowSize.y * 0.95 );
		
		const float progressPercentage = ProgressTracker::getProgress();
		progressBar.setSize( sf::Vector2f( windowSize.x * progressPercentage, windowSize.y * 0.05 ) );
		progressBar.setFillColor( sf::Color( 100 * progressPercentage, 255, 100 * progressPercentage) );
		mainWindow.draw( progressBar );

		mainWindow.popGLStates();

		mainWindow.display();
	}

	void Application::startLongOperation() {
		timedLog->notifyApplicationOnMessage = true;
		ProgressTracker::onMarkFinished = [&] () {
			updateProgress();
		};
	}

	void Application::endLongOperation() {
		timedLog->notifyApplicationOnMessage = false;
		ProgressTracker::onMarkFinished = nullptr;
	}
}

#if 0
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
#endif


// from http://code.msdn.microsoft.com/windowsdesktop/CppUACSelfElevation-981c0160

void ReportError(LPCWSTR pszFunction, DWORD dwError = GetLastError())
{
	wchar_t szMessage[200];
	if (SUCCEEDED(StringCchPrintf(szMessage, ARRAYSIZE(szMessage), 
		L"%s failed w/err 0x%08lx", pszFunction, dwError)))
	{
		MessageBox(NULL, szMessage, L"Error", MB_ICONERROR);
	}
}

#pragma region "Helper Functions for Admin Privileges and Elevation Status"

//
//   FUNCTION: IsUserInAdminGroup()
//
//   PURPOSE: The function checks whether the primary access token of the 
//   process belongs to user account that is a member of the local 
//   Administrators group, even if it currently is not elevated.
//
//   RETURN VALUE: Returns TRUE if the primary access token of the process 
//   belongs to user account that is a member of the local Administrators 
//   group. Returns FALSE if the token does not.
//
//   EXCEPTION: If this function fails, it throws a C++ DWORD exception which 
//   contains the Win32 error code of the failure.
//
//   EXAMPLE CALL:
//     try 
//     {
//         if (IsUserInAdminGroup())
//             wprintf (L"User is a member of the Administrators group\n");
//         else
//             wprintf (L"User is not a member of the Administrators group\n");
//     }
//     catch (DWORD dwError)
//     {
//         wprintf(L"IsUserInAdminGroup failed w/err %lu\n", dwError);
//     }
//
BOOL IsUserInAdminGroup()
{
	BOOL fInAdminGroup = FALSE;
	DWORD dwError = ERROR_SUCCESS;
	HANDLE hToken = NULL;
	HANDLE hTokenToCheck = NULL;
	DWORD cbSize = 0;
	OSVERSIONINFO osver = { sizeof(osver) };

	// Open the primary access token of the process for query and duplicate.
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, 
		&hToken))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Determine whether system is running Windows Vista or later operating 
	// systems (major version >= 6) because they support linked tokens, but 
	// previous versions (major version < 6) do not.
	if (!GetVersionEx(&osver))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	if (osver.dwMajorVersion >= 6)
	{
		// Running Windows Vista or later (major version >= 6). 
		// Determine token type: limited, elevated, or default. 
		TOKEN_ELEVATION_TYPE elevType;
		if (!GetTokenInformation(hToken, TokenElevationType, &elevType, 
			sizeof(elevType), &cbSize))
		{
			dwError = GetLastError();
			goto Cleanup;
		}

		// If limited, get the linked elevated token for further check.
		if (TokenElevationTypeLimited == elevType)
		{
			if (!GetTokenInformation(hToken, TokenLinkedToken, &hTokenToCheck, 
				sizeof(hTokenToCheck), &cbSize))
			{
				dwError = GetLastError();
				goto Cleanup;
			}
		}
	}
	
	// CheckTokenMembership requires an impersonation token. If we just got a 
	// linked token, it already is an impersonation token.  If we did not get 
	// a linked token, duplicate the original into an impersonation token for 
	// CheckTokenMembership.
	if (!hTokenToCheck)
	{
		if (!DuplicateToken(hToken, SecurityIdentification, &hTokenToCheck))
		{
			dwError = GetLastError();
			goto Cleanup;
		}
	}

	// Create the SID corresponding to the Administrators group.
	BYTE adminSID[SECURITY_MAX_SID_SIZE];
	cbSize = sizeof(adminSID);
	if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, &adminSID,  
		&cbSize))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Check if the token to be checked contains admin SID.
	// http://msdn.microsoft.com/en-us/library/aa379596(VS.85).aspx:
	// To determine whether a SID is enabled in a token, that is, whether it 
	// has the SE_GROUP_ENABLED attribute, call CheckTokenMembership.
	if (!CheckTokenMembership(hTokenToCheck, &adminSID, &fInAdminGroup)) 
	{
		dwError = GetLastError();
		goto Cleanup;
	}

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (hToken)
	{
		CloseHandle(hToken);
		hToken = NULL;
	}
	if (hTokenToCheck)
	{
		CloseHandle(hTokenToCheck);
		hTokenToCheck = NULL;
	}

	// Throw the error if something failed in the function.
	if (ERROR_SUCCESS != dwError)
	{
		throw dwError;
	}

	return fInAdminGroup;
}


// 
//   FUNCTION: IsRunAsAdmin()
//
//   PURPOSE: The function checks whether the current process is run as 
//   administrator. In other words, it dictates whether the primary access 
//   token of the process belongs to user account that is a member of the 
//   local Administrators group and it is elevated.
//
//   RETURN VALUE: Returns TRUE if the primary access token of the process 
//   belongs to user account that is a member of the local Administrators 
//   group and it is elevated. Returns FALSE if the token does not.
//
//   EXCEPTION: If this function fails, it throws a C++ DWORD exception which 
//   contains the Win32 error code of the failure.
//
//   EXAMPLE CALL:
//     try 
//     {
//         if (IsRunAsAdmin())
//             wprintf (L"Process is run as administrator\n");
//         else
//             wprintf (L"Process is not run as administrator\n");
//     }
//     catch (DWORD dwError)
//     {
//         wprintf(L"IsRunAsAdmin failed w/err %lu\n", dwError);
//     }
//
BOOL IsRunAsAdmin()
{
	BOOL fIsRunAsAdmin = FALSE;
	DWORD dwError = ERROR_SUCCESS;
	PSID pAdministratorsGroup = NULL;

	// Allocate and initialize a SID of the administrators group.
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (!AllocateAndInitializeSid(
		&NtAuthority, 
		2, 
		SECURITY_BUILTIN_DOMAIN_RID, 
		DOMAIN_ALIAS_RID_ADMINS, 
		0, 0, 0, 0, 0, 0, 
		&pAdministratorsGroup))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Determine whether the SID of administrators group is enabled in 
	// the primary access token of the process.
	if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (pAdministratorsGroup)
	{
		FreeSid(pAdministratorsGroup);
		pAdministratorsGroup = NULL;
	}

	// Throw the error if something failed in the function.
	if (ERROR_SUCCESS != dwError)
	{
		throw dwError;
	}

	return fIsRunAsAdmin;
}


//
//   FUNCTION: IsProcessElevated()
//
//   PURPOSE: The function gets the elevation information of the current 
//   process. It dictates whether the process is elevated or not. Token 
//   elevation is only available on Windows Vista and newer operating 
//   systems, thus IsProcessElevated throws a C++ exception if it is called 
//   on systems prior to Windows Vista. It is not appropriate to use this 
//   function to determine whether a process is run as administartor.
//
//   RETURN VALUE: Returns TRUE if the process is elevated. Returns FALSE if 
//   it is not.
//
//   EXCEPTION: If this function fails, it throws a C++ DWORD exception 
//   which contains the Win32 error code of the failure. For example, if 
//   IsProcessElevated is called on systems prior to Windows Vista, the error 
//   code will be ERROR_INVALID_PARAMETER.
//
//   NOTE: TOKEN_INFORMATION_CLASS provides TokenElevationType to check the 
//   elevation type (TokenElevationTypeDefault / TokenElevationTypeLimited /
//   TokenElevationTypeFull) of the process. It is different from 
//   TokenElevation in that, when UAC is turned off, elevation type always 
//   returns TokenElevationTypeDefault even though the process is elevated 
//   (Integrity Level == High). In other words, it is not safe to say if the 
//   process is elevated based on elevation type. Instead, we should use 
//   TokenElevation.
//
//   EXAMPLE CALL:
//     try 
//     {
//         if (IsProcessElevated())
//             wprintf (L"Process is elevated\n");
//         else
//             wprintf (L"Process is not elevated\n");
//     }
//     catch (DWORD dwError)
//     {
//         wprintf(L"IsProcessElevated failed w/err %lu\n", dwError);
//     }
//
BOOL IsProcessElevated()
{
	BOOL fIsElevated = FALSE;
	DWORD dwError = ERROR_SUCCESS;
	HANDLE hToken = NULL;

	// Open the primary access token of the process with TOKEN_QUERY.
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Retrieve token elevation information.
	TOKEN_ELEVATION elevation;
	DWORD dwSize;
	if (!GetTokenInformation(hToken, TokenElevation, &elevation, 
		sizeof(elevation), &dwSize))
	{
		// When the process is run on operating systems prior to Windows 
		// Vista, GetTokenInformation returns FALSE with the 
		// ERROR_INVALID_PARAMETER error code because TokenElevation is 
		// not supported on those operating systems.
		dwError = GetLastError();
		goto Cleanup;
	}

	fIsElevated = elevation.TokenIsElevated;

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (hToken)
	{
		CloseHandle(hToken);
		hToken = NULL;
	}

	// Throw the error if something failed in the function.
	if (ERROR_SUCCESS != dwError)
	{
		throw dwError;
	}

	return fIsElevated;
}


//
//   FUNCTION: GetProcessIntegrityLevel()
//
//   PURPOSE: The function gets the integrity level of the current process. 
//   Integrity level is only available on Windows Vista and newer operating 
//   systems, thus GetProcessIntegrityLevel throws a C++ exception if it is 
//   called on systems prior to Windows Vista.
//
//   RETURN VALUE: Returns the integrity level of the current process. It is 
//   usually one of these values:
//
//     SECURITY_MANDATORY_UNTRUSTED_RID (SID: S-1-16-0x0)
//     Means untrusted level. It is used by processes started by the 
//     Anonymous group. Blocks most write access. 
//
//     SECURITY_MANDATORY_LOW_RID (SID: S-1-16-0x1000)
//     Means low integrity level. It is used by Protected Mode Internet 
//     Explorer. Blocks write acess to most objects (such as files and 
//     registry keys) on the system. 
//
//     SECURITY_MANDATORY_MEDIUM_RID (SID: S-1-16-0x2000)
//     Means medium integrity level. It is used by normal applications 
//     being launched while UAC is enabled. 
//
//     SECURITY_MANDATORY_HIGH_RID (SID: S-1-16-0x3000)
//     Means high integrity level. It is used by administrative applications 
//     launched through elevation when UAC is enabled, or normal 
//     applications if UAC is disabled and the user is an administrator. 
//
//     SECURITY_MANDATORY_SYSTEM_RID (SID: S-1-16-0x4000)
//     Means system integrity level. It is used by services and other 
//     system-level applications (such as Wininit, Winlogon, Smss, etc.)  
//
//   EXCEPTION: If this function fails, it throws a C++ DWORD exception 
//   which contains the Win32 error code of the failure. For example, if 
//   GetProcessIntegrityLevel is called on systems prior to Windows Vista, 
//   the error code will be ERROR_INVALID_PARAMETER.
//
//   EXAMPLE CALL:
//     try 
//     {
//         DWORD dwIntegrityLevel = GetProcessIntegrityLevel();
//     }
//     catch (DWORD dwError)
//     {
//         wprintf(L"GetProcessIntegrityLevel failed w/err %lu\n", dwError);
//     }
//
DWORD GetProcessIntegrityLevel()
{
	DWORD dwIntegrityLevel = 0;
	DWORD dwError = ERROR_SUCCESS;
	HANDLE hToken = NULL;
	DWORD cbTokenIL = 0;
	PTOKEN_MANDATORY_LABEL pTokenIL = NULL;

	// Open the primary access token of the process with TOKEN_QUERY.
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Query the size of the token integrity level information. Note that 
	// we expect a FALSE result and the last error ERROR_INSUFFICIENT_BUFFER
	// from GetTokenInformation because we have given it a NULL buffer. On 
	// exit cbTokenIL will tell the size of the integrity level information.
	if (!GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &cbTokenIL))
	{
		if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
		{
			// When the process is run on operating systems prior to Windows 
			// Vista, GetTokenInformation returns FALSE with the 
			// ERROR_INVALID_PARAMETER error code because TokenElevation 
			// is not supported on those operating systems.
			dwError = GetLastError();
			goto Cleanup;
		}
	}

	// Now we allocate a buffer for the integrity level information.
	pTokenIL = (TOKEN_MANDATORY_LABEL *)LocalAlloc(LPTR, cbTokenIL);
	if (pTokenIL == NULL)
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Retrieve token integrity level information.
	if (!GetTokenInformation(hToken, TokenIntegrityLevel, pTokenIL, 
		cbTokenIL, &cbTokenIL))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Integrity Level SIDs are in the form of S-1-16-0xXXXX. (e.g. 
	// S-1-16-0x1000 stands for low integrity level SID). There is one and 
	// only one subauthority.
	dwIntegrityLevel = *GetSidSubAuthority(pTokenIL->Label.Sid, 0);

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (hToken)
	{
		CloseHandle(hToken);
		hToken = NULL;
	}
	if (pTokenIL)
	{
		LocalFree(pTokenIL);
		pTokenIL = NULL;
		cbTokenIL = 0;
	}

	// Throw the error if something failed in the function.
	if (ERROR_SUCCESS != dwError)
	{
		throw dwError;
	}

	return dwIntegrityLevel;
}

#pragma endregion

void SelfElevate() {
	BOOL fIsRunAsAdmin; 
	try 
	{ 
		fIsRunAsAdmin = IsRunAsAdmin(); 
	} 
	catch (DWORD dwError) 
	{ 
		ReportError(L"IsRunAsAdmin", dwError); 
		return; 
	} 

	if( fIsRunAsAdmin ) {
		return;
	}

	wchar_t szPath[MAX_PATH]; 
	if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath))) 
	{ 
		// Launch itself as administrator. 
		SHELLEXECUTEINFO sei = { sizeof(sei) }; 
		sei.lpVerb = L"runas"; 
		sei.lpFile = szPath; 
		sei.hwnd = NULL;; 
		sei.nShow = SW_NORMAL; 

		wchar_t szDir[MAX_PATH]; 
		GetCurrentDirectory( ARRAYSIZE(szDir), szDir );
		sei.lpDirectory = szDir;
 
		if (!ShellExecuteEx(&sei)) 
		{ 
			DWORD dwError = GetLastError(); 
			ReportError(L"ShellExecuteEx", dwError); 
		} 
	}
}

void main() {
	MEMORYSTATUSEX memoryStatusEx;
	memoryStatusEx.dwLength = sizeof( memoryStatusEx );
	GlobalMemoryStatusEx( &memoryStatusEx );

	std::cout 
		<< "Memory info\n"
		<< "\tMemory load: " << memoryStatusEx.dwMemoryLoad << "%\n"
		<< "\tTotal physical memory: " << (memoryStatusEx.ullTotalPhys >> 30) << " GB\n"
		<< "\tAvail physical memory: " << (memoryStatusEx.ullAvailPhys >> 30) << " GB\n";

	const int maxLoad = 90;
	if( memoryStatusEx.dwMemoryLoad > maxLoad ) {
		std::cerr << "Memory load over " << maxLoad << "!\n";
		return;
	}

	const size_t minMemory = 500 << 20;
	const size_t saveMemory = 500 << 20;
	const size_t memoryLimit = memoryStatusEx.ullAvailPhys - saveMemory;
	if( memoryLimit < minMemory) {
		std::cerr << "Not enough memory available (" << (minMemory >> 20) << " MB)!\n";
		return;
	}

	HANDLE jobObject = CreateJobObject( NULL, NULL );

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
	info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
	info.ProcessMemoryLimit = memoryLimit;
	if( !SetInformationJobObject( jobObject, JobObjectExtendedLimitInformation, &info, sizeof( info ) ) ) {
		std::cerr << "Failed to set the memory limit!\n";
		return;
	}

	if( !AssignProcessToJobObject( jobObject, GetCurrentProcess() ) ) {
		std::cerr << "Failed to assign the process to its job object! Try to deactivate the application compatibility assistent for Visual Studio 2010!\n";
		//SelfElevate();
		return;
	}

	std::cout << "Memory limit set to " << (memoryLimit >> 20) << " MB\n\n";

	try {
		aop::Application application;

		application.init();

		application.eventLoop();
	}
	catch( std::exception &e) {
		std::cout << e.what() << "\n";
	}
}