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

#include "visualizations.h"

template<>
struct AntTWBarUI::detail::TypeMap< Editor::ModeState > {
	static int Type;
};

int AntTWBarUI::detail::TypeMap< Editor::ModeState >::Type = TW_TYPE_UNDEF;

std::weak_ptr<AntTweakBarEventHandler::GlobalScope> AntTweakBarEventHandler::globalScope;


//////////////////////////////////////////////////////////////////////////

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

	void render() {
		for( auto debugObject = debugObjects.begin() ; debugObject != debugObjects.end() ; ++debugObject ) {
			(*debugObject)->renderScene();
		}
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
	struct SceneDisplayListObject : IDebugObject {
		// TODO: scoped objects should accept nonscoped objects as move targets! [10/15/2012 kirschan2]
		GL::DisplayList displayList;

		DebugUI *debugUI;

		AntTWBarUI::SimpleContainer container;

		bool visible;

		SceneDisplayListObject( const std::string &name, GL::DisplayList &displayList )
			: container( AntTWBarUI::CT_GROUP, name )
			, displayList( displayList )
			, visible( true )
		{
			init();
		}

		~SceneDisplayListObject() {
			displayList.release();
		}

		void init() {
			container.add( AntTWBarUI::makeSharedVariable( "Visible", AntTWBarUI::makeReferenceAccessor( visible ) ) );
			container.add( AntTWBarUI::makeSharedButton( "Remove",
				[&] () {
					debugUI->remove( this );
				}
			) );
		}

		virtual void link( DebugUI *debugUI, Tag ) {
			this->debugUI = debugUI;
		}

		virtual AntTWBarUI::Element::SPtr getUI( Tag ) {
			return make_nonallocated_shared( container );
		}

		virtual void renderScene( Tag ) {
			if( visible ) {
				displayList.call();
			}
		}
	};

	struct SGSRenderer : IDebugObject {
		Application *application;

		DebugUI *debugUI;

		AntTWBarUI::SimpleContainer container;

		SGSRenderer( Application *application ) : container( "SGSSceneRenderer" ), application( application ) {
			init();
		}

		void init() {
			container.add( AntTWBarUI::makeSharedVariable(
				"Hide scene",
				AntTWBarUI::makeReferenceAccessor( application->hideScene )
			) );
			container.add( AntTWBarUI::makeSharedVariable(
				"Show scene as wireframe",
				AntTWBarUI::makeReferenceAccessor( application->world->sceneRenderer.debug.showSceneWireframe )
			) );
			container.add( AntTWBarUI::makeSharedVariable(
				"Show bounding boxes",
				AntTWBarUI::makeReferenceAccessor( application->world->sceneRenderer.debug.showBoundingSpheres )
			) );
			container.add( AntTWBarUI::makeSharedVariable(
				"Show terrain bounding boxes",
				AntTWBarUI::makeReferenceAccessor( application->world->sceneRenderer.debug.showTerrainBoundingSpheres )
			) );
			container.add( AntTWBarUI::makeSharedVariable(
				"Update render lists",
				AntTWBarUI::makeReferenceAccessor( application->world->sceneRenderer.debug.updateRenderLists )
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

	struct ProbeDatabase : IDebugObject {
		Application *application;

		DebugUI *debugUI;

		AntTWBarUI::SimpleContainer container;

		Vector3f skyColor;

		ProbeDatabase( Application *application )
			: container( "Probe database" )
			, application( application )
			, skyColor( 0.5, 1.0, 0.5 )
		{
			init();
		}

		void visualizeModel( int modelIndex, ProbeVisualizationMode pvm ) {
			if( application->probeDatabase.isEmpty( modelIndex ) ) {
				return;
			}

			// TODO: add an instanceIndex member to each instance in ProbeDatabase [10/16/2012 kirschan2]
			const auto instanceIndices = application->world->sceneRenderer.getModelInstances( modelIndex );
			int counter = 0;
			for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex, ++counter ) {
				const auto transformation = application->world->sceneRenderer.getInstanceTransformation( *instanceIndex );

				DebugRender::setTransformation( transformation );
				DebugRender::startLocalTransform();
				visualizeProbeDataset(
					skyColor,
					application->sceneSettings.probeGenerator_maxDistance,
					application->sceneSettings.probeGenerator_resolution,
					application->probeDatabase.getIdDatasets()[ modelIndex ].getProbes(),
					application->probeDatabase.getIdDatasets()[ modelIndex ].getInstances()[ counter ].getProbeContexts(),
					pvm
				);
				DebugRender::endLocalTransform();
			}
		}

		void visualizeAll( ProbeVisualizationMode pvm ) {
			DebugRender::begin();

			const int numIds = application->probeDatabase.getNumIdDatasets();
			for( int modelIndex = 0 ; modelIndex < numIds ; ++modelIndex ) {
				visualizeModel( modelIndex, pvm );
			}

			DebugRender::end();
		}

		void addVisualization( ProbeVisualizationMode pvm, const std::string &name ) {
			GL::ScopedDisplayList list;
			list.begin();

			visualizeAll( pvm );

			list.end();

			debugUI->add(
				std::make_shared< DebugObjects::SceneDisplayListObject >( name, list.publish() )
			);
		}

		void standaloneVisualization( const std::string &name ) {
			auto window = std::make_shared< MultiDisplayListVisualizationWindow >();
			// render the scene as wireframe
			{
				auto &modelVisualization = window->visualizations[0];
				modelVisualization.name = "scene (wireframe)";
				window->makeVisible( 0 );
				modelVisualization.displayList.create();
				modelVisualization.displayList.begin();
				application->world->sceneRenderer.renderFullScene( true );
				modelVisualization.displayList.end();
			}
			// render the scene
			{
				auto &modelVisualization = window->visualizations[9];
				modelVisualization.name = "scene (normal)";
				modelVisualization.displayList.create();
				modelVisualization.displayList.begin();
				application->world->sceneRenderer.renderFullScene( false );
				modelVisualization.displayList.end();
			}
			// render the samples using position colors
			{
				auto &probeVisualization = window->visualizations[1];
				probeVisualization.name = "color samples";
				probeVisualization.displayList.create();
				probeVisualization.displayList.begin();
				visualizeAll( PVM_COLOR );
				probeVisualization.displayList.end();
			}
			// render the samples using hits
			{
				auto &probeVisualization = window->visualizations[2];
				probeVisualization.name = "occlusion samples";
				probeVisualization.displayList.create();
				probeVisualization.displayList.begin();
				visualizeAll( PVM_OCCLUSION );
				probeVisualization.displayList.end();
			}
			// render the samples using hits
			{
				auto &probeVisualization = window->visualizations[3];
				probeVisualization.name = "distance samples";
				probeVisualization.displayList.create();
				probeVisualization.displayList.begin();
				visualizeAll( PVM_DISTANCE );
				probeVisualization.displayList.end();
			}

			window->keyLogics[1].disableMask = window->keyLogics[2].disableMask = window->keyLogics[3].disableMask = 2+4+8;
			window->keyLogics[0].disableMask = window->keyLogics[9].disableMask = 512 + 1;

			window->init( name );

			application->debugWindowManager.add( window );
		}

		void visualizeMaxDistance() {
			GL::ScopedDisplayList list;
			list.begin();

			DebugRender::begin();
			DebugRender::setColor( Vector3f( 1.0, 1.0, 0.0 ) );

			const int numIds = application->probeDatabase.getNumIdDatasets();
			for( int modelIndex = 0 ; modelIndex < numIds ; ++modelIndex ) {
				if( application->probeDatabase.isEmpty( modelIndex ) ) {
					continue;
				}

				// TODO: add an instanceIndex member to each instance in ProbeDatabase [10/16/2012 kirschan2]
				const auto instanceIndices = application->world->sceneRenderer.getModelInstances( modelIndex );
				for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
					const auto transformation = application->world->sceneRenderer.getInstanceTransformation( *instanceIndex );

					DebugRender::setPosition( transformation.translation() );
					DebugRender::drawAbstractSphere( application->sceneSettings.probeGenerator_maxDistance );
				}
			}

			DebugRender::end();

			list.end();

			debugUI->add(
				std::make_shared< DebugObjects::SceneDisplayListObject >( "ProbeGenerator maxDistance", list.publish() )
			);
		}

		void init() {
			container.add( AntTWBarUI::makeSharedButton(
				"Max distance for all sampled instances",
				[&] () {
					visualizeMaxDistance();
				}
			) );

			/*container.add( AntTWBarUI::makeSharedVariableWithConfig<AntTWBarUI::VariableConfigs::ForcedType< TW_TYPE_COLOR3F >(
				"Sky color replacement",
				makeReferenceAccessor<float*>( &skyColor[0] )
			) );*/
			container.add( EigenColor3fUI().makeShared(
				AntTWBarUI::makeReferenceAccessor( skyColor ),
				AntTWBarUI::CT_GROUP,
				"Sky color replacement"
			) );

			container.add( AntTWBarUI::makeSharedButton(
				"color for all sampled instances",
				[&] () {
					addVisualization( PVM_COLOR, "Probe database (Color)" );
				}
			) );
			container.add( AntTWBarUI::makeSharedButton(
				"occlusion for all sampled instances",
				[&] () {
					addVisualization( PVM_OCCLUSION, "Probe database (Occlusion)" );
				}
			) );
			container.add( AntTWBarUI::makeSharedButton(
				"distance for all sampled instances",
				[&] () {
					addVisualization( PVM_DISTANCE, "Probe database (Distance)" );
				}
			) );
			container.add( AntTWBarUI::makeSharedSeparator() );
			container.add( AntTWBarUI::makeSharedButton(
				"standalone visualization",
				[&] () {
					standaloneVisualization( "Probe database" );
				}
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

	struct OptixView : IDebugObject {
		Application *application;

		DebugUI *debugUI;

		AntTWBarUI::SimpleContainer container;

		OptixView( Application *application ) : container( "Optix" ), application( application ) {
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

		ModelDatabase( Application *application ) : container( AntTWBarUI::CT_GROUP, "ModelDatabase" ), application( application ) {
			init();
		}

		void init() {
			container.setName( "Model Database" );
			for( int modelIndex = 0 ; modelIndex < application->modelDatabase.informationById.size() ; modelIndex++ ) {
				container.add( AntTWBarUI::makeSharedButton(
					application->modelDatabase.informationById[ modelIndex ].shortName,
					[&, modelIndex] () {
						const auto &modelInformation = application->modelDatabase.informationById[ modelIndex ];

						// TODO: it would be nice to add this as rendered displaylist to the debug viz window [10/16/2012 kirschan2]
						// display some debug output
						log(
							boost::format(
								"Model information %i '%s':\n"
								"\tvolume: %f\n"
								"\tarea: %f\n"
								"\tdiagonalLength: %f\n"
								"\tvoxelResolution: %f"
							)
							% modelIndex
							% modelInformation.name
							% modelInformation.volume
							% modelInformation.area
							% modelInformation.diagonalLength
							% modelInformation.voxelResolution
						);

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
							visualizeColorGrid( modelInformation.voxels, GVM_POSITION );
							voxelVisualization.displayList.end();
						}
						// render the samples using hits
						{
							auto &voxelVisualization = window->visualizations[2];
							voxelVisualization.name = "voxels (colored using overdraw)";
							voxelVisualization.displayList.create();
							voxelVisualization.displayList.begin();
							visualizeColorGrid( modelInformation.voxels, GVM_HITS );
							voxelVisualization.displayList.end();
						}
						// render the samples using hits
						{
							auto &voxelVisualization = window->visualizations[3];
							voxelVisualization.name = "voxels (colored using normals)";
							voxelVisualization.displayList.create();
							voxelVisualization.displayList.begin();
							visualizeColorGrid( modelInformation.voxels, GVM_NORMAL );
							voxelVisualization.displayList.end();
						}
						// render the samples using samples
						{
							auto &voxelVisualization = window->visualizations[4];
							voxelVisualization.name = "probes";
							voxelVisualization.displayList.create();
							voxelVisualization.displayList.begin();
							visualizeProbes(
								modelInformation.voxels.getMapping().getResolution(),
								modelInformation.probes
							);
							voxelVisualization.displayList.end();
						}

						window->keyLogics[1].disableMask = window->keyLogics[2].disableMask = window->keyLogics[3].disableMask = 2+4+8;


						window->init( modelInformation.name );

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
#if 0
	struct ProbeDatabaseUI {
		Application *application;

		struct DatasetView : AntTWBarUI::SimpleStructureFactory< std::string, DatasetView > {
			ProbeDatabaseUI *probeDatabaseUI;

			DatasetView( ProbeDatabaseUI *probeDatabaseUI )
				: probeDatabaseUI( probeDatabaseUI )
			{
			}

			template< typename ElementAccessor >
			void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
				container->add( AntTWBarUI::makeSharedLabel(
					AntTWBarUI::makeExpressionAccessor<std::string>(
						[&] () -> std::string {
							return probeDatabaseUI->application->modelDatabase.informationById[ accessor.elementIndex ].shortName;
						}
					),
				) );
				container->add( AntTWBarUI::makeSharedReadOnlyVariable( "numInstances",
					AntTWBarUI::CallbackAccessor<int>(
						[&] ( int &shadow ) {
							shadow = probeDatabaseUI->application->probeDatabase.getIdDatasets()[ accessor.]
						}
					)
				) );
			}
		};

		AntTWBarUI::SimpleContainer ui;
		ProbeDatabaseUI( Application *application )
			: application( application )
			, ui( "Probe Database" )
		{
			auto datasetsContainer = AntTWBarUI::makeSharedVector( )
		}

		void refresh() {
			ui.refresh();
		}
	};
#endif
	struct ModelDatabaseUI {
		Application *application;

		AntTWBarUI::SimpleContainer ui;

		ModelDatabaseUI( Application *application ) : application( application ) {
			init();
		}

		void init() {
			ui.setName( "Model Database" );
			ui.add( AntTWBarUI::makeSharedButton( "Load", [&] () { application->modelDatabase.load( application->settings.modelDatabasePath.c_str() ); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store", [&] () { application->modelDatabase.store( application->settings.modelDatabasePath.c_str() ); } ) );
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

namespace aop {
	struct Application::NamedVolumesEditorView : Editor::Volumes {
		std::vector< aop::SceneSettings::NamedTargetVolume > &volumes;

		NamedVolumesEditorView( std::vector< aop::SceneSettings::NamedTargetVolume > &volumes  ) : volumes( volumes ) {}

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
			AntTWBarUI::detail::TypeMap< Editor::ModeState >::Type =
				AntTWBarUI::TypeBuilder::Enum<Editor::ModeState>( "EditorStateEnum" )
					.add( "Freelook", Editor::M_FREELOOK )
					.add( "Selecting", Editor::M_SELECTING )
					.add( "Placing", Editor::M_PLACING )
					.add( "Moving", Editor::M_MOVING )
					.add( "Rotating", Editor::M_ROTATING )
					.add( "Resizing", Editor::M_RESIZING )
					.define();
			;

			ui.setName( "aop" );

			ui.add( AntTWBarUI::makeSharedButton( 
				"Load settings",
				[this] {
					LocalCandidateBarUI::Candidates candidates;
					candidates.push_back( LocalCandidateBarUI::ScoreModelIndexPair( 1.0, 0 ) );
					candidates.push_back( LocalCandidateBarUI::ScoreModelIndexPair( 0.5, 1 ) );

					application->localCandidateBarUIs.emplace_back(
						std::make_shared<LocalCandidateBarUI>( application, candidates, Obb( Affine3f( Matrix3f::Identity() ), Vector3f::Constant( 10.0 ) ) )
					);
				}
			) );

			ui.add( AntTWBarUI::makeSharedVariable(
				"Editor mode",
				AntTWBarUI::CallbackAccessor<Editor::ModeState>(
					[&] ( Editor::ModeState &v ) { v = application->editor.currentMode; },
					[&] ( const Editor::ModeState &v ) { application->editor.selectMode( v ); }
				)
			));
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedVariable(
				"Probe maxDistance",
				AntTWBarUI::makeReferenceAccessor( application->sceneSettings.probeGenerator_maxDistance )
			) );
			ui.add( AntTWBarUI::makeSharedVariable(
				"Probe resolution",
				AntTWBarUI::makeReferenceAccessor( application->sceneSettings.probeGenerator_resolution )
			) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Load settings", [this] { application->sceneSettings.load( application->settings.sceneSettingsPath.c_str() ); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store settings", [this] { application->sceneSettings.store( application->settings.sceneSettingsPath.c_str() ); } ) );
			//ui.add( AntTWBarUI::makeSharedVector< NamedTargetVolumeView >( "Camera Views", application->settings.volumes) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Sample marked objects", [this] {
				application->startLongOperation();

				const auto &modelIndices = application->modelTypesUI->markedModels;

				ProgressTracker::Context progressTracker( modelIndices.size() + 1 );
				for( auto modelIndex = modelIndices.begin() ; modelIndex != modelIndices.end() ; ++modelIndex ) {
					application->ProbeDatabase_sampleInstances( *modelIndex );
					progressTracker.markFinished();
				}

				application->probeDatabase.integrateDatasets();
				progressTracker.markFinished();

				application->endLongOperation();
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedVariable(
				"Probe query occlusion tolerance",
				AntTWBarUI::makeReferenceAccessor( application->sceneSettings.probeQuery_occlusionTolerance )
			) );
			ui.add( AntTWBarUI::makeSharedVariable(
				"Probe query distance tolerance",
				AntTWBarUI::makeReferenceAccessor( application->sceneSettings.probeQuery_distanceTolerance )
			) );
			ui.add( AntTWBarUI::makeSharedVariable(
				"Probe query color tolerance",
				AntTWBarUI::makeReferenceAccessor( application->sceneSettings.probeQuery_colorTolerance )
			) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
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

						std::vector<CandidateSidebarUI::ScoreModelIndexPair> scoredModelIndices;
						for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
							scoredModelIndices.push_back( std::make_pair( matchInfo->score, matchInfo->id ) );
						}

						application->candidateSidebarUI->setModels( scoredModelIndices, selection->getObb().transformation.translation() );
						application->localCandidateBarUIs.emplace_back(
							std::make_shared<LocalCandidateBarUI>( application, scoredModelIndices, selection->getObb() )
						);
					}
				};
				QueryVolumeVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Query volume (weighted)", [this] () {
				struct QueryVolumeVisitor : Editor::SelectionVisitor {
					Application *application;

					QueryVolumeVisitor( Application *application ) : application( application ) {}

					void visit() {
						std::cerr << "No volume selected!\n";
					}
					void visit( Editor::ObbSelection *selection ) {
						application->startLongOperation();
						auto matchInfos = application->weightedQueryVolume( selection->getObb() );
						application->endLongOperation();

						typedef ProbeDatabase::WeightedQuery::MatchInfo MatchInfo;
						boost::sort(
							matchInfos,
							[] (const MatchInfo &a, MatchInfo &b ) {
								return a.score > b.score;
							}
						);

						std::vector<CandidateSidebarUI::ScoreModelIndexPair> scoredModelIndices;
						for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
							scoredModelIndices.push_back( std::make_pair( matchInfo->score, matchInfo->id ) );
						}

						application->candidateSidebarUI->setModels( scoredModelIndices, selection->getObb().transformation.translation() );
					}
				};
				QueryVolumeVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedVariable(
				"Neighborhood max distance",
				AntTWBarUI::makeReferenceAccessor( application->sceneSettings.neighborhoodDatabase_maxDistance )
			) );
			ui.add( AntTWBarUI::makeSharedVariable(
				"Neighborhood query tolerance",
				AntTWBarUI::makeReferenceAccessor( application->sceneSettings.neighborhoodDatabase_queryTolerance )
			) );
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
							application->sceneSettings.neighborhoodDatabase_maxDistance,
							application->sceneSettings.neighborhoodDatabase_queryTolerance
						);
						application->endLongOperation();

						application->candidateSidebarUI->setModels( queryResults, selection->getObb().transformation.translation() );
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
							application->sceneSettings.neighborhoodDatabase_maxDistance,
							application->sceneSettings.neighborhoodDatabase_queryTolerance
						);
						application->endLongOperation();

						application->candidateSidebarUI->setModels( queryResults, selection->getObb().transformation.translation() );
					}
				};
				QueryNeighborsVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Query volume & neighbors", [this] () {
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

						/*typedef ProbeDatabase::WeightedQuery::MatchInfo MatchInfo;
						boost::sort(
							matchInfos,
							[] (const MatchInfo &a, MatchInfo &b ) {
								return a.score > b.score;
							}
						);*/

						application->startLongOperation();
						auto queryResults = queryVolumeNeighborsV2(
							application->world.get(),
							application->neighborDatabaseV2,
							selection->getObb().transformation.translation(),
							application->sceneSettings.neighborhoodDatabase_maxDistance,
							application->sceneSettings.neighborhoodDatabase_queryTolerance
						);
						application->endLongOperation();

						std::vector< std::pair< float, int > > scores( application->modelDatabase.informationById.size() );

						// merge both results
						for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
							scores[ matchInfo->id ] = std::make_pair( matchInfo->score, matchInfo->id );
						}

						for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
							if( scores[ queryResult->second ].first > 0 ) {
								scores[ queryResult->second ].first += queryResult->first * 0.5;
								// we might disable the if-check again, so here.. make it always work correctly :)
								scores[ queryResult->second ].second = queryResult->second;
							}
						}

						boost::sort( scores, std::greater< std::pair< float, int > >() );
						boost::remove_erase_if( scores, [] ( const std::pair< float, int > &x ) { return x.first == 0.0; } );

						application->candidateSidebarUI->setModels( scores, selection->getObb().transformation.translation() );
					}
				};
				QueryVolumeVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Load probe database", [this] { application->probeDatabase.loadCache( application->settings.probeDatabasePath.c_str() ); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Reset probe database", [this] { application->probeDatabase.clear(); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store probe database", [this] { application->probeDatabase.storeCache( application->settings.probeDatabasePath.c_str() ); } ) );
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

		// init the debug ui
		debugUI = std::make_shared< DebugUI >();

		// add some default objects to the debug ui
		debugUI->add( std::make_shared< DebugObjects::SGSRenderer >( this ) );
		debugUI->add( std::make_shared< DebugObjects::ProbeDatabase >( this ) );
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

		world->init( settings.scenePath.c_str() );
	}

	// this just fills the model database with whatever data we can quickly extract from the scene
	void Application::ModelDatabase_init() {
		const auto &models = world->scene.models;
		const int numModels = models.size();

		ProgressTracker::Context progressTracker( numModels );

		for( int modelId = 0 ; modelId < numModels ; modelId++ ) {
			const auto bbox = world->sceneRenderer.getModelBoundingBox( modelId );

			ModelDatabase::ModelInformation idInformation;

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

			modelDatabase.modelNameIdMap[ idInformation.name ] = modelId;
			modelDatabase.informationById.emplace_back( std::move( idInformation ) );

		}

		debugUI->add( std::make_shared< DebugObjects::ModelDatabase >( this ) );
	}

	int Application::ModelDatabase_sampleModel( int modelId, float resolution ) {
		AUTO_TIMER( boost::format( "model %i") % modelId );

		const auto bbox = world->sceneRenderer.getModelBoundingBox( modelId );

		ModelDatabase::ModelInformation & idInformation = modelDatabase.informationById[ modelId ];

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
					// use the trivial normal calculations for now [10/16/2012 kirschan2]
#if 0
					const Vector3f normal(
						sample.nx / 255.0 * 2 - 1.0,
						sample.ny / 255.0 * 2 - 1.0,
						sample.nz / 255.0 * 2 - 1.0
					);
#else
					// we dont use the weighted and averaged normal for now
					const Vector3f normal = (position - bbox.center()).normalized();
#endif
					ProbeGenerator::appendProbesFromSample( position, normal, probes );
				}
			}
		}

		probes.shrink_to_fit();

		const int count = voxels.getMapping().count;

		log( boost::format(
			"Ratio %f = %i / %i (% i probes)" )
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

		int totalNonEmpty = 0;
		int totalCounts = 0;
		int totalProbes = 0;

		for( int modelId = 0 ; modelId < numModels ; modelId++ ) {
			const int numNonEmpty = ModelDatabase_sampleModel( modelId, sceneSettings.probeGenerator_resolution );

			ModelDatabase::ModelInformation & idInformation = modelDatabase.informationById[ modelId ];

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

	void Application::ProbeDatabase_sampleInstances( int modelIndex ) {
		AUTO_TIMER_FOR_FUNCTION();
		log( boost::format( "sampling model %i" ) % modelIndex );

		const auto &probes = modelDatabase.getProbes( modelIndex, sceneSettings.probeGenerator_resolution );
		if( probes.empty() ) {
			logError( boost::format( "empty model %i: '%s'!" ) % modelIndex % modelDatabase.informationById[ modelIndex ].shortName );
			return;
		}

		RenderContext renderContext;
		renderContext.setDefault();
		//renderContext.disabledModelIndex = modelIndex;

		auto instanceIndices = world->sceneRenderer.getModelInstances( modelIndex );

		ProgressTracker::Context progressTracker( instanceIndices.size() * 3 );

		int totalCount = 0;

		for( int i = 0 ; i < instanceIndices.size() ; i++ ) {
			const int instanceIndex = instanceIndices[ i ];

			std::vector<SGSInterface::Probe> transformedProbes;
			{
				const auto &transformation = world->sceneRenderer.getInstanceTransformation( instanceIndex );
				ProbeGenerator::transformProbes( probes, transformation, transformedProbes );
			}

			RawProbeDataset rawDataset;

			progressTracker.markFinished();

			AUTO_TIMER_BLOCK( boost::str( boost::format( "sampling probe batch with %i probes for instance %i" ) % probes.size() % instanceIndex ) ) {
				renderContext.disabledInstanceIndex = instanceIndex;
				world->optixRenderer.sampleProbes( transformedProbes, rawDataset, renderContext, sceneSettings.probeGenerator_maxDistance, instanceIndex );
			}
			progressTracker.markFinished();

			probeDatabase.addDataset(modelIndex, probes, InstanceProbeDataset( rawDataset ) );
			progressTracker.markFinished();

			totalCount += (int) transformedProbes.size();
		}

		log( boost::format( "total sampled probes: %i" ) % totalCount );
	}

	ProbeContextTolerance Application::getPCTFromSettings() {
		ProbeContextTolerance pct;
		pct.occusionTolerance = sceneSettings.probeQuery_occlusionTolerance;
		pct.distanceTolerance = sceneSettings.probeQuery_distanceTolerance;
		pct.colorLabTolerance = sceneSettings.probeQuery_colorTolerance;
		return pct;
	}

	ProbeDatabase::Query::MatchInfos Application::queryVolume( const Obb &queryVolume ) {
		ProgressTracker::Context progressTracker(4);

		AUTO_TIMER_FOR_FUNCTION();

		RenderContext renderContext;
		renderContext.setDefault();

		std::vector<OptixProgramInterface::Probe> probes;

		ProbeGenerator::generateQueryProbes( queryVolume, sceneSettings.probeGenerator_resolution, probes );

		progressTracker.markFinished();

		RawProbeDataset rawDataset;
		AUTO_TIMER_BLOCK( "sampling scene") {
			world->optixRenderer.sampleProbes( probes, rawDataset, renderContext, sceneSettings.probeGenerator_maxDistance );
		}
		progressTracker.markFinished();

		ProbeDatabase::Query query( probeDatabase );
		{
			query.setQueryDataset( std::move( rawDataset ) );

			query.setProbeContextTolerance( getPCTFromSettings() );

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

	ProbeDatabase::WeightedQuery::MatchInfos Application::weightedQueryVolume( const Obb &queryVolume ) {
		ProgressTracker::Context progressTracker(4);

		AUTO_TIMER_FOR_FUNCTION();

		RenderContext renderContext;
		renderContext.setDefault();

		std::vector<OptixProgramInterface::Probe> probes;
		ProbeGenerator::generateQueryProbes( queryVolume, sceneSettings.probeGenerator_resolution, probes );

		progressTracker.markFinished();

		RawProbeDataset rawDataset;
		{
			AUTO_TIMER_FOR_FUNCTION( "sampling scene");
			world->optixRenderer.sampleProbes( probes, rawDataset, renderContext, sceneSettings.probeGenerator_maxDistance );
		}
		progressTracker.markFinished();

		ProbeDatabase::WeightedQuery query( probeDatabase );
		query.setQueryDataset( std::move( probes ), std::move( rawDataset ) );

		query.setProbeContextTolerance( getPCTFromSettings() );

		query.execute();

		auto matchInfos = query.getCandidates();

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
				% matchInfo->numMatches
				% matchInfo->probeMatchPercentage
				% matchInfo->queryMatchPercentage
				% matchInfo->score
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

		settings.load();

		initMainWindow();
		initCamera();
		initEventHandling();

		initSGSInterface();

		// TODO: fix parameter order [10/10/2012 kirschan2]
#if 1
		sampleAllNeighbors( sceneSettings.neighborhoodDatabase_maxDistance, neighborDatabase, *world );
		sampleAllNeighborsV2( sceneSettings.neighborhoodDatabase_maxDistance, neighborDatabaseV2, *world );
#endif

		probeDatabase.reserveIds( world->scene.modelNames.size() );

		namedVolumesEditorView.reset( new NamedVolumesEditorView( sceneSettings.volumes ) );

		initEditor();

		eventDispatcher.addEventHandler( make_nonallocated_shared( widgetRoot ) );

		// register anttweakbar first because it actually manages its own focus
		antTweakBarEventHandler.init( mainWindow );
		eventDispatcher.addEventHandler( make_nonallocated_shared( antTweakBarEventHandler ) );

		initUI();

		ModelDatabase_init();
		neighborDatabaseV2.modelDatabase = &modelDatabase;

		// load sceneSettings
		sceneSettings.load( settings.sceneSettingsPath.c_str() );

		if( !sceneSettings.views.empty() ) {
			sceneSettings.views.front().pushTo( mainCamera );
		}

		if( !sceneSettings.volumes.empty() ) {
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

		for( auto localCandidateBarUI = localCandidateBarUIs.begin() ; localCandidateBarUI != localCandidateBarUIs.end() ; ++localCandidateBarUI ) {
			(*localCandidateBarUI)->refresh();
		}
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

			timedLog->updateTime( clock.getElapsedTime().asSeconds() );
			timedLog->updateText();

			// update the UIs
			eventSystem.update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );
			cameraView.updateFromCamera( mainCamera );
			updateUI();			

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

				if( !hideScene) {
					world->renderViewFrame( cameraView );
				}

				debugUI->render();

				// render target volumes
				// render obbs
				DebugRender::begin();
				DebugRender::setColor( Eigen::Vector3f::Constant( 1.0 ) );
				for( auto namedObb = sceneSettings.volumes.begin() ; namedObb != sceneSettings.volumes.end() ; ++namedObb ) {
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
		aop::SceneSettings::NamedTargetVolume targetVolume;

		targetVolume.name = "test";
		targetVolume.volume.transformation.setIdentity();
		targetVolume.volume.size.setConstant( 3.0 );

		application.settings.volumes.push_back( targetVolume );
	}

	application.eventLoop();
};
#endif


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
		std::cerr << "Failed to assign the process to its job object! Try to deactivate the application compatibility assistant for Visual Studio 2010!\n";
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