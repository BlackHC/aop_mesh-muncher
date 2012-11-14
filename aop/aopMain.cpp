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

#include "validation.h"
#include "boost/random/mersenne_twister.hpp"
#include "boost/random/uniform_on_sphere.hpp"
#include "boost/random/uniform_real_distribution.hpp"

#pragma warning( once: 4244 )

boost::mt19937 randomNumberGenerator(static_cast<unsigned int>(std::time(0)));

std::weak_ptr<AntTweakBarEventHandler::GlobalScope> AntTweakBarEventHandler::globalScope;

//////////////////////////////////////////////////////////////////////////
// V2
//
//
void sampleAllNeighborsForV2( std::vector<int> modelIndices, float maxDistance, Neighborhood::NeighborhoodDatabaseV2 &database, SGSInterface::World &world ) {
	AUTO_TIMER_FOR_FUNCTION();

	for( auto modelIndex = modelIndices.begin() ; modelIndex != modelIndices.end() ; ++modelIndex ) {
		const auto instanceIndices = world.sceneRenderer.getModelInstances( *modelIndex );

		for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
			auto queryResults = world.sceneGrid.query(
				-1,
				*instanceIndex,
				world.sceneRenderer.getInstanceTransformation( *instanceIndex ).translation(),
				maxDistance
			);

			const int modelIndex = world.sceneRenderer.getModelIndex( *instanceIndex );

			database.addInstance( modelIndex, std::move( queryResults ) );
		}
	}
}

void sampleAllNeighborsV2( float maxDistance, Neighborhood::NeighborhoodDatabaseV2 &database, SGSInterface::World &world ) {
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

		database.addInstance( modelIndex, std::move( queryResults ) );
	}
}

Neighborhood::Results queryVolumeNeighborsV2( SGSInterface::World *world, Neighborhood::NeighborhoodDatabaseV2 &database, const Vector3f &position, float maxDistance, float tolerance ) {
	AUTO_TIMER_FOR_FUNCTION();

	auto sceneQueryResults = world->sceneGrid.query( -1, -1, position, maxDistance );

	Neighborhood::NeighborhoodDatabaseV2::Query query( database, tolerance, std::move( sceneQueryResults ) );
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
		for( auto debugObject = debugObjects.rbegin() ; debugObject != debugObjects.rend() ; ++debugObject ) {
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

		SceneDisplayListObject( const std::string &name, GL::DisplayList &displayList, bool visible = true )
			: container( AntTWBarUI::CT_GROUP, name )
			, displayList( displayList )
			, visible( visible )
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

		Vector3f missColor;
		static bool automaticallyVisualizeQuery;
		static bool automaticallyVisualizeConfigurationQueryDetails;

		ProbeDatabase( Application *application )
			: container( "Probe database" )
			, application( application )
			, missColor( 0.2, 0.3, 0.4 )
		{
			init();
		}

		void visualizeModel( int localModelIndex, ProbeVisualizationMode pvm ) {
			const auto &sampledModel = application->probeDatabase.getSampledModels()[ localModelIndex ];
			if( sampledModel.isEmpty() ) {
				return;
			}

			// TODO: add an instanceIndex member to each instance in ProbeDatabase [10/16/2012 kirschan2]
			for( auto sampledInstance = sampledModel.getInstances().begin() ; sampledInstance != sampledModel.getInstances().end() ; ++sampledInstance ) {
				DebugRender::setTransformation( sampledInstance->getSource() );
				DebugRender::startLocalTransform();
				visualizeProbeDataset(
					missColor,
					application->sceneSettings.probeGenerator_maxDistance,
					application->sceneSettings.probeGenerator_resolution,
					1.0,
					sampledModel.getProbes(),
					sampledInstance->getProbeSamples(),
					pvm
				);
				DebugRender::endLocalTransform();
			}
		}

		void visualizeAll( ProbeVisualizationMode pvm ) {
			DebugRender::begin();

			const int numLocalModels = application->probeDatabase.getNumSampledModels();
			for( int localModelIndex = 0 ; localModelIndex < numLocalModels ; ++localModelIndex ) {
				visualizeModel( localModelIndex, pvm );
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

		template< typename FullQueryType >
		void addConfigurationQueryVisualization( const Obb &volume, const FullQueryType &query ) {
			const float scaleFactor = 0.25f;
			const auto details = query.getDetailedQueryResults();

			// only do the first two models for now
			int numDetails = std::min<int>( details.size(), 2 );
			for( int detailIndex = 0 ; detailIndex < numDetails ; detailIndex++ ) {
				const auto &detailedQueryResult = details[ detailIndex ];

				const float bestScore = detailedQueryResult.score;

				const int sceneModelIndex = detailedQueryResult.sceneModelIndex;
				const int localModelIndex = query.database.modelIndexMapper.getLocalModelIndex( sceneModelIndex );
				const int numProbes = query.database.getSampledModels()[ localModelIndex ].getProbes().size();

#if 1
				GL::ScopedDisplayList list;
				list.begin();

				DebugRender::begin();
				DebugRender::setTransformation( volume.transformation );
				DebugRender::startLocalTransform();

				for( int z = 0 ; z < query.queryVolumeSize[2] ; z++ ) {
					for( int y = 0 ; y < query.queryVolumeSize[1] ; y++ ) {
						for( int x = 0 ; x < query.queryVolumeSize[0] ; x++ ) {
							const Vector3f position = query.queryResolution * (Vector3i( x, y, z ) - query.queryVolumeOffset).cast<float>();
							DebugRender::setPosition( position );

							const int index = x + query.queryVolumeSize[0] * (y + query.queryVolumeSize[1] * z);

							int bestOrientation = 0;
							float bestCount = 0.0f;
							for( int orientationIndex = 0 ; orientationIndex < ProbeGenerator::getNumOrientations() ; orientationIndex++ ) {
								const auto &counterGrid = detailedQueryResult.matchesByOrientation[ orientationIndex ];
								const auto matchCount = counterGrid[ index ];
								if( matchCount > bestCount ) {
									bestCount = matchCount;
									bestOrientation = orientationIndex;
								}
							}

							const float normalizedScore = bestCount / numProbes / bestScore;
							if( normalizedScore < 0.5 ) {
								continue;
							}

							const float redComponent = clamp<float>( normalizedScore, 0.0f, 1.0f );
							const float glowComponent = clamp<float>( (normalizedScore - 0.9f) * 10.0f, 0.0f, 1.0f );
							DebugRender::setColor( Vector3f( redComponent, glowComponent, glowComponent ) );

							DebugRender::drawAbstractSphere( scaleFactor / 4 * query.queryResolution, true, 5 );
							DebugRender::drawVector( scaleFactor * query.queryResolution * -ProbeGenerator::getRotation(bestOrientation).col(2) );
						}
					}
				}

				DebugRender::setPosition( query.queryResolution * query.queryVolumeOffset.cast<float>() + Vector3f::Constant( 4.0 ) );
				DebugRender::drawCoordinateSystem( 2.0 );
				DebugRender::endLocalTransform();
				DebugRender::end();
				list.end();
				debugUI->add(
					std::make_shared< DebugObjects::SceneDisplayListObject >(
						boost::str(
							boost::format( "%s" )
							% query.database.modelIndexMapper.getSceneModelName( sceneModelIndex )
						),
						list.publish(),
						false
					)
				);
#else
				for( int orientationIndex = 0 ; orientationIndex < ProbeGenerator::getNumOrientations() ; orientationIndex++ ) {
					const auto &counterGrid = detailedQueryResult.matchesByOrientation[ orientationIndex ];

					GL::ScopedDisplayList list;
					list.begin();

					DebugRender::begin();
					DebugRender::setTransformation( volume.transformation );
					DebugRender::startLocalTransform();

					for( int z = 0 ; z < query.queryVolumeSize[2] ; z++ ) {
						for( int y = 0 ; y < query.queryVolumeSize[1] ; y++ ) {
							for( int x = 0 ; x < query.queryVolumeSize[0] ; x++ ) {
								const Vector3f position = query.queryResolution * (Vector3i( x, y, z ) - query.queryVolumeOffset).cast<float>();
								DebugRender::setPosition( position );

								const int index = x + query.queryVolumeSize[0] * (y + query.queryVolumeSize[1] * z);

								const float normalizedScore = float( counterGrid[ index ] ) / numProbes;
								const float redComponent = clamp<float>( normalizedScore / 5.0f, 0.0f, 1.0f );
								const float glowComponent = clamp<float>( normalizedScore / 5.0f - 1.0, 0.0f, 1.0f );
								DebugRender::setColor( Vector3f( redComponent, glowComponent, glowComponent ) );
								DebugRender::drawAbstractSphere( scaleFactor * query.queryResolution, 5 );
							}
						}
					}

					DebugRender::setPosition( query.queryResolution * query.queryVolumeOffset.cast<float>() + Vector3f::Constant( 4.0 ) );
					DebugRender::drawCoordinateSystem( 2.0 );
					DebugRender::endLocalTransform();
					DebugRender::end();
					list.end();
					debugUI->add(
						std::make_shared< DebugObjects::SceneDisplayListObject >(
							boost::str(
								boost::format( "%s %i" )
								% query.database.modelIndexMapper.getSceneModelName( sceneModelIndex )
								% orientationIndex
							),
							list.publish(),
							false
						)
					);
				}
#endif
			}
		}

		void addQueryVisualization( const SceneSettings::NamedTargetVolume &queryVolume, const ProbeContext::RawProbes &probes, const ProbeContext::RawProbeSamples &probeSamples ) {
			const float scaleFactor = 0.25f;
			{
				GL::ScopedDisplayList list;
				list.begin();

				DebugRender::begin();
				DebugRender::setTransformation( queryVolume.volume.transformation );

				visualizeRawProbeSamples(
					missColor,
					application->sceneSettings.probeGenerator_maxDistance,
					application->sceneSettings.probeGenerator_resolution,
					scaleFactor,
					probes,
					probeSamples,
					PVM_COLOR
				);

				DebugRender::end();

				list.end();
				debugUI->add(
					std::make_shared< DebugObjects::SceneDisplayListObject >(
						boost::str( boost::format( "Query '%s' color" ) % queryVolume.name ),
						list.publish()
					)
				);
			}
			{
				GL::ScopedDisplayList list;
				list.begin();

				DebugRender::begin();
				DebugRender::setTransformation( queryVolume.volume.transformation );

				visualizeRawProbeSamples(
					missColor,
					application->sceneSettings.probeGenerator_maxDistance,
					application->sceneSettings.probeGenerator_resolution,
					scaleFactor,
					probes,
					probeSamples,
					PVM_DISTANCE
				);

				DebugRender::end();

				list.end();
				debugUI->add(
					std::make_shared< DebugObjects::SceneDisplayListObject >(
						boost::str( boost::format( "Query '%s' distance" ) % queryVolume.name ),
						list.publish()
					)
				);
			}
			{
				GL::ScopedDisplayList list;
				list.begin();

				DebugRender::begin();
				DebugRender::setTransformation( queryVolume.volume.transformation );

				visualizeRawProbeSamples(
					missColor,
					application->sceneSettings.probeGenerator_maxDistance,
					application->sceneSettings.probeGenerator_resolution,
					scaleFactor,
					probes,
					probeSamples,
					PVM_OCCLUSION
				);

				DebugRender::end();

				list.end();
				debugUI->add(
					std::make_shared< DebugObjects::SceneDisplayListObject >(
						boost::str( boost::format( "Query '%s' occlusion" ) % queryVolume.name ),
						list.publish()
					)
				);
			}
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
				application->world->renderStandaloneFrame( application->cameraView, true );
				modelVisualization.displayList.end();
			}
			// render the scene
			{
				auto &modelVisualization = window->visualizations[9];
				modelVisualization.name = "scene (normal)";
				modelVisualization.displayList.create();
				modelVisualization.displayList.begin();
				application->world->renderStandaloneFrame( application->cameraView, false );
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

			window->camera = application->mainCamera;

			application->debugWindowManager.add( window );
		}

		void visualizeMaxDistance() {
			GL::ScopedDisplayList list;
			list.begin();

			DebugRender::begin();
			DebugRender::setColor( Vector3f( 1.0, 1.0, 0.0 ) );

			const int numSampledModels = application->probeDatabase.getNumSampledModels();
			for( int localModelIndex = 0 ; localModelIndex < numSampledModels ; ++localModelIndex ) {
				const auto &sampledModel = application->probeDatabase.getSampledModels()[ localModelIndex ];

				for( auto instance = sampledModel.getInstances().begin() ; instance != sampledModel.getInstances().end() ; instance++ ) {
					const auto &transformation = instance->getSource();

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
			container.add( AntTWBarUI::makeSharedVariable( "Auto vis query", AntTWBarUI::makeReferenceAccessor( automaticallyVisualizeQuery ) ) );
			container.add( AntTWBarUI::makeSharedVariable( "Auto vis config query", AntTWBarUI::makeReferenceAccessor( automaticallyVisualizeConfigurationQueryDetails ) ) );
			container.add( AntTWBarUI::makeSharedButton(
				"Vis max distance for all sampled instances",
				[&] () {
					visualizeMaxDistance();
				}
			) );

			/*container.add( AntTWBarUI::makeSharedVariableWithConfig<AntTWBarUI::VariableConfigs::ForcedType< TW_TYPE_COLOR3F >(
				"Sky color replacement",
				makeReferenceAccessor<float*>( &skyColor[0] )
			) );*/
			container.add( EigenColor3fUI().makeShared(
				AntTWBarUI::makeReferenceAccessor( missColor ),
				AntTWBarUI::CT_GROUP,
				"Sky color replacement"
			) );

			container.add( AntTWBarUI::makeSharedButton(
				"Vis color for all sampled instances",
				[&] () {
					addVisualization( PVM_COLOR, "Probe database (Color)" );
				}
			) );
			container.add( AntTWBarUI::makeSharedButton(
				"Vis occlusion for all sampled instances",
				[&] () {
					addVisualization( PVM_OCCLUSION, "Probe database (Occlusion)" );
				}
			) );
			container.add( AntTWBarUI::makeSharedButton(
				"Vis distance for all sampled instances",
				[&] () {
					addVisualization( PVM_DISTANCE, "Probe database (Distance)" );
				}
			) );
			container.add( AntTWBarUI::makeSharedSeparator() );
			container.add( AntTWBarUI::makeSharedButton(
				"Vis standalone visualization",
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

	bool ProbeDatabase::automaticallyVisualizeQuery = false;
	bool ProbeDatabase::automaticallyVisualizeConfigurationQueryDetails = false;

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
		aop::NormalGenerationMode normalGenerationMode;

		Application *application;

		AntTWBarUI::SimpleContainer ui;

		ModelDatabaseUI( Application *application )
			: application( application )
			, normalGenerationMode( aop::NGM_COMBINED )
		{
			init();
		}

		void init() {
			AntTWBarUI::TypeBuilder::Enum<aop::NormalGenerationMode>( "NormalGenerationMode" )
				.add( "Position", aop::NGM_POSITION )
				.add( "Average Normal", aop::NGM_AVERAGE_NORMAL )
				.add( "Neighbors", aop::NGM_NEIGHBORS )
				.add( "Combined", aop::NGM_COMBINED )
				.define();
			;

			ui.setName( "Model Database" );
			ui.add( AntTWBarUI::makeSharedVariable( "Normal generation mode", AntTWBarUI::makeReferenceAccessor( normalGenerationMode  ) ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Load", [&] () { application->modelDatabase.load( application->settings.modelDatabasePath.c_str() ); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store", [&] () { application->modelDatabase.store( application->settings.modelDatabasePath.c_str() ); } ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Sample models", [&] {
				application->startLongOperation();
				application->ModelDatabase_sampleAll( normalGenerationMode );
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
	struct ModelSelectionBarUI {
		Application *application;

		ClippedContainer clipper;
		ScrollableContainer scroller;

		ModelSelectionBarUI( Application *application )
			: application( application  )
		{
			init();
		}

		void init() {
			application->widgetRoot.addEventHandler( make_nonallocated_shared( clipper ) );
			clipper.addEventHandler( make_nonallocated_shared( scroller ) );

			const float buttonWidth = 0.1f;
			const float buttonHeight = buttonWidth * ViewportContext::context->getAspectRatio();

			scroller.size[0] = 1.0f;
			scroller.size[1] = buttonHeight;
			scroller.verticalScrollByDefault = false;
			scroller.scrollStep = buttonWidth;

			clipper.transformChain.setOffset( Eigen::Vector2f( 0.0f, 1.0f - buttonHeight ) );

			const size_t numModels = application->world->scene.models.size();
			for( size_t modelIndex = 0; modelIndex < numModels ; modelIndex++ ) {
				scroller.addEventHandler(
					std::make_shared< ActionModelButton >(
						Eigen::Vector2f( buttonWidth * modelIndex, 0.0 ),
						Eigen::Vector2f( buttonWidth, buttonHeight ),
						modelIndex,
						application->world->sceneRenderer,
						[this, modelIndex] () {
							log( application->world->scene.modelNames[ modelIndex ] );

							if( sf::Keyboard::isKeyPressed( sf::Keyboard::LShift ) ) {
								application->editor.selectAdditionalModel( modelIndex );
							}
							else {
								application->editor.selectModel( modelIndex );
							}
						}
					)
				);
			}

			scroller.updateScrollArea();
			clipper.updateLocalArea();
		}

		void refresh() {

		}
	};

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
		QueryType queryType;

		MainUI( Application *application ) : application( application ), queryType( QT_NORMAL ) {
			init();
		}

		void init() {
			AntTWBarUI::TypeBuilder::Enum<Editor::ModeState>( "EditorStateEnum" )
				.add( "Freelook", Editor::M_FREELOOK )
				.add( "Selecting", Editor::M_SELECTING )
				.add( "Placing", Editor::M_PLACING )
				.add( "Moving", Editor::M_MOVING )
				.add( "Rotating", Editor::M_ROTATING )
				.add( "Resizing", Editor::M_RESIZING )
				.define();
			;

			AntTWBarUI::TypeBuilder::Enum< QueryType >( "QueryType" )
				.add( "Normal", QT_NORMAL )
				.add( "Importance", QT_IMPORTANCE )
				.add( "Configuration", QT_FULL )
				.add( "Importance Configuration", QT_IMPORTANCE_FULL )
				.add( "Fast Normal", QT_FAST_QUERY )
				.add( "Fast Importance", QT_FAST_IMPORTANCE )
				.add( "Fast Configuration", QT_FAST_FULL )
				.define()
			;

			ui.setName( "aop" );

			ui.add( AntTWBarUI::makeSharedButton(
				"Save scene",
				[this] {
					application->world->sceneRenderer.makeAllInstancesStatic();
					{
						Serializer::BinaryWriter writer( application->settings.scenePath.c_str() );
						Serializer::write( writer, application->world->scene );
					}
				}
			) );

			ui.add( AntTWBarUI::makeSharedButton(
				"Make all objects dynamic",
				[this] {
					application->world->sceneRenderer.makeAllInstancesDynamic();
				}
			) );
			ui.add( AntTWBarUI::makeSharedButton(
				"Make all objects static",
				[this] {
					application->world->sceneRenderer.makeAllInstancesStatic();
				}
			) );

#if 0
			ui.add( AntTWBarUI::makeSharedButton(
				"Test local candidate bars",
				[this] {
					LocalCandidateBarUI::Candidates candidates;
					candidates.push_back( LocalCandidateBarUI::ScoreModelIndexPair( 1.0f, 0 ) );
					candidates.push_back( LocalCandidateBarUI::ScoreModelIndexPair( 0.5f, 1 ) );

					application->localCandidateBarUIs.emplace_back(
						std::make_shared<LocalCandidateBarUI>( application, candidates, Obb( Affine3f( Matrix3f::Identity() ), Vector3f::Constant( 10.0 ) ) )
					);
				}
			) );
#endif

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
			ui.add( AntTWBarUI::makeSharedButton( "Load settings", [this] {
				application->sceneSettings.load( application->settings.sceneSettingsPath.c_str() );
				application->modelTypesUI->loadFromSceneSettings();
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store settings", [this] {
				application->settings.store();
				application->sceneSettings.store( application->settings.sceneSettingsPath.c_str() );
			} ) );

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

				application->probeDatabase.compileAll( application->sceneSettings.probeGenerator_maxDistance );
				progressTracker.markFinished();

				application->endLongOperation();
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Remerge models (recompress)", [this] {
				application->startLongOperation();

				const auto &modelIndices = application->modelTypesUI->markedModels;

				ProgressTracker::Context progressTracker( modelIndices.size() + 1 );
				application->probeDatabase.compileAll( application->sceneSettings.probeGenerator_maxDistance );
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
			ui.add( AntTWBarUI::makeSharedVariable( "Query type", AntTWBarUI::makeReferenceAccessor( queryType ) ) );
			ui.add( AntTWBarUI::makeSharedButton( "Query selected volume", [this] () {
				struct QueryVolumeVisitor : Editor::SelectionVisitor {
					Application *application;
					Application::QueryType queryType;

					QueryVolumeVisitor( Application *application, Application::QueryType queryType )
						: application( application )
						, queryType( queryType )
					{}

					void visit() {
						std::cerr << "No volume selected!\n";
					}

					void visit( Editor::ObbSelection *selection ) {
						application->startLongOperation();
						auto queryResults = application->queryVolume( application->sceneSettings.volumes[ selection->index ], queryType );
						application->endLongOperation();

						application->candidateSidebarUI->setModels( queryResults );
						application->localCandidateBarUIs.emplace_back(
							std::make_shared<LocalCandidateBarUI>( application, queryResults, selection->getObb() )
						);
					}
				};
				QueryVolumeVisitor( application, queryType ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Query all volumes", [this] () {
				application->startLongOperation();
				ProgressTracker::Context progressTracker( application->sceneSettings.volumes.size() );
				for( auto queryVolume = application->sceneSettings.volumes.begin() ; queryVolume != application->sceneSettings.volumes.end() ; ++queryVolume ) {
					auto queryResults = application->queryVolume( *queryVolume, queryType );

					boost::sort(
						queryResults,
						QueryResult::greaterByScoreAndModelIndex
					);

					application->localCandidateBarUIs.emplace_back(
						std::make_shared<LocalCandidateBarUI>( application, queryResults, queryVolume->volume )
					);

					progressTracker.markFinished();
				}
				application->endLongOperation();
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
			ui.add( AntTWBarUI::makeSharedButton( "Query neighbors V2", [this] () {
				struct QueryNeighborsVisitor : Editor::SelectionVisitor {
					Application *application;

					QueryNeighborsVisitor( Application *application ) : application( application ) {}

					void visit() {
						logError( "No volume selected!\n" );
					}
					void visit( Editor::ObbSelection *selection ) {
						application->startLongOperation();
						Neighborhood::Results queryResults = queryVolumeNeighborsV2(
							application->world.get(),
							application->neighborDatabaseV2,
							selection->getObb().transformation.translation(),
							application->sceneSettings.neighborhoodDatabase_maxDistance,
							application->sceneSettings.neighborhoodDatabase_queryTolerance
						);
						boost::sort( queryResults, std::greater< Neighborhood::Result >() );
						application->endLongOperation();

						QueryResults transformedQueryResults;
						for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
							QueryResult transformedQueryResult;
							transformedQueryResult.score = queryResult->first;
							transformedQueryResult.sceneModelIndex = queryResult->second;
							transformedQueryResult.transformation = selection->getObb().transformation;
							transformedQueryResults.push_back( transformedQueryResult );
						}

						application->candidateSidebarUI->setModels( transformedQueryResults );
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
						auto matchInfos = application->queryVolume( application->sceneSettings.volumes[ selection->index ], Application::QT_NORMAL );
						application->endLongOperation();

						/*typedef ProbeDatabase::WeightedQuery::MatchInfo MatchInfo;
						boost::sort(
							matchInfos,
							[] (const MatchInfo &a, MatchInfo &b ) {
								return a.score > b.score;
							}
						);*/

						application->startLongOperation();
						Neighborhood::Results queryResults = queryVolumeNeighborsV2(
							application->world.get(),
							application->neighborDatabaseV2,
							selection->getObb().transformation.translation(),
							application->sceneSettings.neighborhoodDatabase_maxDistance,
							application->sceneSettings.neighborhoodDatabase_queryTolerance
						);
						boost::sort( queryResults, std::greater< Neighborhood::Result >() );
						application->endLongOperation();

						std::vector< std::pair< float, int > > scores( application->modelDatabase.informationById.size() );

						// merge both results
						for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
							scores[ queryResult->second ] = std::make_pair( queryResult->first, queryResult->second );
						}

						for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
							if( scores[ matchInfo->sceneModelIndex ].first > 0.0f ) {
								matchInfo->score += scores[ matchInfo->sceneModelIndex ].first * 0.5f;
							}
						}

						boost::sort(
							matchInfos,
							QueryResult::greaterByScoreAndModelIndex
						);

						application->candidateSidebarUI->setModels( matchInfos );
					}
				};
				QueryVolumeVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Load probe database", [this] {
				application->probeDatabase.load( application->settings.probeDatabasePath );
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Reset probe database", [this] {
				application->probeDatabase.clearAll();
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store probe database", [this] {
				application->probeDatabase.store( application->settings.probeDatabasePath );
			} ) );

			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Load neighborhood database", [this] {
				application->neighborDatabaseV2.load( application->settings.neighborhoodDatabaseV2Path );
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Clear neighborhood database", [this] {
				application->neighborDatabaseV2.clear();
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "All // sample neighborhood database", [this] {
				application->neighborDatabaseV2.clear();
				sampleAllNeighborsV2( application->sceneSettings.neighborhoodDatabase_maxDistance, application->neighborDatabaseV2, *application->world );
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Marked // sample neighborhood database", [this] {
				application->neighborDatabaseV2.clear();
				sampleAllNeighborsForV2( application->modelTypesUI->markedModels, application->sceneSettings.neighborhoodDatabase_maxDistance, application->neighborDatabaseV2, *application->world );
			} ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store neighborhood database", [this] {
				application->neighborDatabaseV2.store( application->settings.neighborhoodDatabaseV2Path );
			} ) );

			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedVariable(
				"position variance // neighborhood validation",
				AntTWBarUI::makeReferenceAccessor( application->settings.validation_neighborhood_positionVariance )
			) );
			ui.add( AntTWBarUI::makeSharedVariable(
				"#samples // neighborhood validation",
				AntTWBarUI::makeReferenceAccessor( application->settings.validation_neighborhood_numSamples )
			) );
			ui.add( AntTWBarUI::makeSharedButton( "create validation file // neighborhood", [this] {
				application->NeighborhoodValidation_queryAllInstances( application->settings.neighborhoodValidationDataPath );
			} ) );

			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedVariable(
				"position variance // probe validation",
				AntTWBarUI::makeReferenceAccessor( application->settings.validation_probes_positionVariance )
			) );
			ui.add( AntTWBarUI::makeSharedVariable(
				"#samples // probe validation",
				AntTWBarUI::makeReferenceAccessor( application->settings.validation_probes_numSamples )
			) );
			ui.add( AntTWBarUI::makeSharedButton( "determine fit bounding box", [this] {
				application->ProbesValidation_determineQueryVolumeSizeForMarkedModels();
			} ) );
			ui.add( AntTWBarUI::makeSharedVariable(
				"query volume size // probe validation",
				AntTWBarUI::makeReferenceAccessor( application->settings.validation_probes_queryVolumeSize )
			) );
			ui.add( AntTWBarUI::makeSharedButton( "create validation file // probes", [this] {
				application->ProbesValidation_queryAllInstances( application->settings.probeValidationDataPath );
			} ) );

			ui.add( AntTWBarUI::makeSharedButton( "print max bounding box diagonal @ marked models", [this] {
				float maxDiagonalLength = 0.0f;
				const auto &modelIndices = application->modelTypesUI->markedModels;
				for( auto modelIndex = modelIndices.begin() ; modelIndex != modelIndices.end() ; ++modelIndex ) {
					maxDiagonalLength =
						std::max(
							maxDiagonalLength,
							application->modelDatabase.informationById[ *modelIndex ].diagonalLength
						)
					;
				}
				log( boost::format( "print max bounding box diagonal @ marked models = %f" ) % maxDiagonalLength );
			} ) );

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

		modelSelectionBarUI = std::make_shared< ModelSelectionBarUI >( this );

		// init the debug ui
		debugUI = std::make_shared< DebugUI >();

		// add some default objects to the debug ui
		debugUI->add( std::make_shared< DebugObjects::SGSRenderer >( this ) );
		probeDatabase_debugUI = std::make_shared< DebugObjects::ProbeDatabase >( this );
		debugUI->add( probeDatabase_debugUI );

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
		glClearColor( 0.2, 0.3, 0.4, 1.0 );
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

	int Application::ModelDatabase_sampleModel( int sceneModelIndex, float resolution, NormalGenerationMode normalGenerationMode ) {
		AUTO_TIMER( boost::format( "model %i") % sceneModelIndex );

		const auto bbox = world->sceneRenderer.getModelBoundingBox( sceneModelIndex );

		ModelDatabase::ModelInformation & idInformation = modelDatabase.informationById[ sceneModelIndex ];

		idInformation.voxelResolution = resolution;

		const auto &voxels = idInformation.voxels = AUTO_TIME( world->sceneRenderer.voxelizeModel( sceneModelIndex, resolution ), "voxelizing");
		GridStorage<int> directionMasks( voxels.getMapping() );

		auto &probes = idInformation.probes;
		probes.reserve( voxels.getMapping().count * 13 );
		probes.clear();

		size_t numNonEmpty = 0;
		AUTO_TIMER_BLOCK( "creating probes" ) {
			for( auto iter = voxels.getIterator() ; iter.hasMore() ; ++iter ) {
				const auto &sample = voxels[ *iter ];

				if( sample.numSamples > 0 ) {
					numNonEmpty++;

					switch( normalGenerationMode ) {
						case NGM_COMBINED:
						case NGM_AVERAGE_NORMAL: {
							// unpack the normal
							const Vector3f normal(
								sample.nx / 255.0 * 2 - 1.0,
								sample.ny / 255.0 * 2 - 1.0,
								sample.nz / 255.0 * 2 - 1.0
							);

							directionMasks[ *iter ] = ProbeGenerator::cullDirectionMask( normal, ~0 );
						}
						break;
						case NGM_POSITION: {
							const Vector3f position = voxels.getMapping().getPosition( iter.getIndex3() );
							const Vector3f normal = (position - bbox.center()).normalized();

							directionMasks[ *iter ] = ProbeGenerator::cullDirectionMask( normal, ~0 );
						}
						break;
						case NGM_NEIGHBORS: {
							directionMasks[ *iter ] = ~0;
						}
						break;
					}
				}
				/*else {
					directionMasks[ *iter ] = 0;
				}*/
			}

			for( auto iter = directionMasks.getIterator() ; iter.hasMore() ; ++iter ) {
				int directionMask = directionMasks[ *iter ];

				if( directionMask == 0 ) {
					continue;
				}

				if( normalGenerationMode == NGM_COMBINED || normalGenerationMode == NGM_NEIGHBORS ) {
					for( int neighborIndex = 0 ; neighborIndex < 26 ; neighborIndex++ ) {
						int neighborBit = 1 << neighborIndex;

						if( (directionMask & neighborBit) == 0 ) {
							continue;
						}

						const Vector3i neighborIndex3 = iter.getIndex3() + neighborOffsets[ neighborIndex ];
						if( directionMasks.getMapping().isValid( neighborIndex3 ) ) {
							const int neighborDirectionMask = directionMasks[ neighborIndex3 ];

							if( neighborDirectionMask & neighborBit ) {
								directionMask &= ~neighborBit;
							}
						}
					}
				}

				const Vector3f position = directionMasks.getMapping().getPosition( iter.getIndex3() );
				ProbeGenerator::appendProbesFromSample( resolution, position, directionMask, probes );
			}
		}

		probes.shrink_to_fit();

		const int count = (int) voxels.getMapping().count;

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
	void Application::ModelDatabase_sampleAll( NormalGenerationMode normalGenerationMode ) {
		AUTO_TIMER();

		const auto &models = world->scene.models;
		const int numModels = (int) models.size();

		ProgressTracker::Context progressTracker( numModels );

		size_t totalNonEmpty = 0;
		size_t totalCounts = 0;
		size_t totalProbes = 0;

		for( int sceneModelIndex = 0 ; sceneModelIndex < numModels ; sceneModelIndex++ ) {
			const int numNonEmpty = ModelDatabase_sampleModel( sceneModelIndex, sceneSettings.probeGenerator_resolution, normalGenerationMode );

			ModelDatabase::ModelInformation & idInformation = modelDatabase.informationById[ sceneModelIndex ];

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

			OptixProgramInterface::TransformedProbes transformedProbes;
			{
				const auto &transformation = world->sceneRenderer.getInstanceTransformation( instanceIndex );
				ProbeGenerator::transformProbes( probes, transformation, sceneSettings.probeGenerator_resolution, transformedProbes );
			}

			// TODO: rename RawProbeData to Optix...::ProbeSamples! [10/21/2012 kirschan2]
			ProbeContext::RawProbeSamples rawProbeSamples;

			progressTracker.markFinished();

			AUTO_TIMER_BLOCK( boost::str( boost::format( "sampling probe batch with %i probes for instance %i" ) % probes.size() % instanceIndex ) ) {
				renderContext.disabledInstanceIndex = instanceIndex;
				world->optixRenderer.sampleProbes( transformedProbes, rawProbeSamples, renderContext, sceneSettings.probeGenerator_maxDistance, instanceIndex + rand() );
			}
			progressTracker.markFinished();

			probeDatabase.addInstanceProbes(
				modelIndex,
				world->sceneRenderer.getInstanceTransformation( instanceIndex ),
				sceneSettings.probeGenerator_resolution,
				probes,
				rawProbeSamples
			);
			progressTracker.markFinished();

			totalCount += (int) transformedProbes.size();
		}

		log( boost::format( "total sampled probes: %i" ) % totalCount );
	}

	ProbeContext::ProbeContextTolerance Application::getPCTFromSettings() {
		ProbeContext::ProbeContextTolerance pct;
		pct.occusionTolerance = sceneSettings.probeQuery_occlusionTolerance;
		pct.distanceTolerance = sceneSettings.probeQuery_distanceTolerance;
		pct.colorLabTolerance = sceneSettings.probeQuery_colorTolerance;
		return pct;
	}

	QueryResults Application::queryVolume( const SceneSettings::NamedTargetVolume &queryVolume, QueryType queryType ) {
		ProgressTracker::Context progressTracker( 3 );

		AUTO_TIMER_FOR_FUNCTION();

		RenderContext renderContext;
		renderContext.setDefault();

		ProbeContext::RawProbes queryProbes;
		ProbeGenerator::generateQueryProbes( queryVolume.volume.size, sceneSettings.probeGenerator_resolution, queryProbes );

		progressTracker.markFinished();

		ProbeContext::RawProbeSamples queryProbeSamples;
		AUTO_TIMER_BLOCK( "sampling scene") {
			OptixRenderer::TransformedProbes transformedQueryProbes;
			ProbeGenerator::transformProbes( queryProbes, queryVolume.volume.transformation, sceneSettings.probeGenerator_resolution, transformedQueryProbes );
			world->optixRenderer.sampleProbes( transformedQueryProbes, queryProbeSamples, renderContext, sceneSettings.probeGenerator_maxDistance );
		}
		progressTracker.markFinished();

		QueryResults queryResults;
		switch( queryType ) {
		case QT_NORMAL:
			queryResults = normalQueryVolume( queryVolume.volume, queryProbes, queryProbeSamples );
			break;
		case QT_IMPORTANCE:
			queryResults = importanceQueryVolume( queryVolume.volume, queryProbes, queryProbeSamples );
			break;
		case QT_FULL:
			queryResults = fullQueryVolume( queryVolume.volume, queryProbes, queryProbeSamples );
			break;
		case QT_IMPORTANCE_FULL:
			queryResults = importanceFullQueryVolume( queryVolume.volume, queryProbes, queryProbeSamples );
			break;
		case QT_FAST_QUERY:
			queryResults = fastNormalQueryVolume( queryVolume.volume, queryProbes, queryProbeSamples );
			break;
		case QT_FAST_IMPORTANCE:
			queryResults = fastImportanceQueryVolume( queryVolume.volume, queryProbes, queryProbeSamples );
			break;
		case QT_FAST_FULL:
			queryResults = fastFullQueryVolume( queryVolume.volume, queryProbes, queryProbeSamples );
			break;
		}
		progressTracker.markFinished();

		if( DebugObjects::ProbeDatabase::automaticallyVisualizeQuery ) {
			probeDatabase_debugUI->addQueryVisualization( queryVolume, queryProbes, queryProbeSamples );
		}

		boost::sort(
			queryResults,
			QueryResult::greaterByScoreAndModelIndex
		);

		return queryResults;
	}

	QueryResults Application::fastNormalQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples ) {
		ProbeContext::ProbeDatabase::FastQuery query( probeDatabase );
		{
			query.setQueryDataset( queryProbeSamples );
			query.setQueryVolume( queryVolume, sceneSettings.probeGenerator_resolution );

			query.setProbeContextTolerance( getPCTFromSettings() );

			query.execute();
		}

		const auto &detailedQueryResults = query.getDetailedQueryResults();
		for( auto detailedQueryResult = detailedQueryResults.begin() ; detailedQueryResult != detailedQueryResults.end() ; ++detailedQueryResult ) {
			log(
				boost::format(
					"%i:"
					"\tdbMatchPercentage %f\n"
					"\tqueryMatchPercentage %f\n"
					"\tscore %f\n"
				)
				% detailedQueryResult->sceneModelIndex
				% detailedQueryResult->probeMatchPercentage
				% detailedQueryResult->queryMatchPercentage
				% detailedQueryResult->score
			);
		}

		return query.getQueryResults();
	}

	QueryResults Application::fastImportanceQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples ) {
		ProbeContext::ProbeDatabase::FastImportanceQuery query( probeDatabase );
		{
			query.setQueryDataset( queryProbeSamples );
			query.setQueryVolume( queryVolume, sceneSettings.probeGenerator_resolution );

			query.setProbeContextTolerance( getPCTFromSettings() );

			query.execute();
		}

		auto detailedQueryResults = query.getDetailedQueryResults();
		for( auto detailedQueryResult = detailedQueryResults.begin() ; detailedQueryResult != detailedQueryResults.end() ; ++detailedQueryResult ) {
			log(
				boost::format(
					"%i:"
					"\tdbMatchPercentage %f\n"
					"\tqueryMatchPercentage %f\n"
					"\tscore %f\n"
				)
				% detailedQueryResult->sceneModelIndex
				% detailedQueryResult->probeMatchPercentage
				% detailedQueryResult->queryMatchPercentage
				% detailedQueryResult->score
			);
		}

		return query.getQueryResults();
	}

	QueryResults Application::fastFullQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples ) {
		ProbeContext::ProbeDatabase::FastConfigurationQuery query( probeDatabase );
		{
			query.setQueryVolume( queryVolume, sceneSettings.probeGenerator_resolution );
			query.setQueryDataset( queryProbes, queryProbeSamples );

			query.setProbeContextTolerance( getPCTFromSettings() );

			query.execute();
		}

		const auto &queryResults = query.getQueryResults();
		for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
			log(
				boost::format(
					"%i:\n"
					"\tscore %f\n"
				)
				% queryResult->sceneModelIndex
				% queryResult->score
			);
		}

		if( DebugObjects::ProbeDatabase::automaticallyVisualizeConfigurationQueryDetails ) {
			probeDatabase_debugUI->addConfigurationQueryVisualization( queryVolume, query );
		}

		return query.getQueryResults();
	}

	QueryResults Application::normalQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples ) {
		ProbeContext::ProbeDatabase::Query query( probeDatabase );
		{
			query.setQueryDataset( queryProbeSamples );
			query.setQueryVolume( queryVolume, sceneSettings.probeGenerator_resolution );

			query.setProbeContextTolerance( getPCTFromSettings() );

			query.execute();
		}

		const auto &detailedQueryResults = query.getDetailedQueryResults();
		for( auto detailedQueryResult = detailedQueryResults.begin() ; detailedQueryResult != detailedQueryResults.end() ; ++detailedQueryResult ) {
			log(
				boost::format(
					"%i:"
					"\tnumMatches %f\n"
					"\tdbMatchPercentage %f\n"
					"\tqueryMatchPercentage %f\n"
					"\tscore %f\n"
				)
				% detailedQueryResult->sceneModelIndex
				% detailedQueryResult->numMatches
				% detailedQueryResult->probeMatchPercentage
				% detailedQueryResult->queryMatchPercentage
				% detailedQueryResult->score
			);
		}

		return query.getQueryResults();
	}

	QueryResults Application::fullQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples ) {
		ProbeContext::ProbeDatabase::FullQuery query( probeDatabase );
		{
			query.setQueryVolume( queryVolume, sceneSettings.probeGenerator_resolution );
			query.setQueryDataset( queryProbes, queryProbeSamples );

			query.setProbeContextTolerance( getPCTFromSettings() );

			query.execute();
		}

		const auto &queryResults = query.getQueryResults();
		for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
			log(
				boost::format(
					"%i:\n"
					"\tscore %f\n"
				)
				% queryResult->sceneModelIndex
				% queryResult->score
			);
		}

		if( DebugObjects::ProbeDatabase::automaticallyVisualizeConfigurationQueryDetails ) {
			probeDatabase_debugUI->addConfigurationQueryVisualization( queryVolume, query );
		}

		return query.getQueryResults();
	}

	QueryResults Application::importanceFullQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples ) {
		ProbeContext::ProbeDatabase::ImportanceFullQuery query( probeDatabase );
		{
			query.setQueryVolume( queryVolume, sceneSettings.probeGenerator_resolution );
			query.setQueryDataset( queryProbes, queryProbeSamples );

			query.setProbeContextTolerance( getPCTFromSettings() );

			query.execute();
		}

		const auto &queryResults = query.getQueryResults();
		for( auto queryResult = queryResults.begin() ; queryResult != queryResults.end() ; ++queryResult ) {
			log(
				boost::format(
					"%i:\n"
					"\tscore %f\n"
				)
				% queryResult->sceneModelIndex
				% queryResult->score
			);
		}

		if( DebugObjects::ProbeDatabase::automaticallyVisualizeConfigurationQueryDetails ) {
			probeDatabase_debugUI->addConfigurationQueryVisualization( queryVolume, query );
		}

		return query.getQueryResults();
	}

	QueryResults Application::importanceQueryVolume( const Obb &queryVolume, const ProbeContext::RawProbes &queryProbes, const ProbeContext::RawProbeSamples &queryProbeSamples ) {
		ProbeContext::ProbeDatabase::ImportanceQuery query( probeDatabase );
		{
			query.setQueryDataset( queryProbeSamples );
			query.setQueryVolume( queryVolume, sceneSettings.probeGenerator_resolution );

			query.setProbeContextTolerance( getPCTFromSettings() );

			query.execute();
		}

		auto detailedQueryResults = query.getDetailedQueryResults();
		for( auto detailedQueryResult = detailedQueryResults.begin() ; detailedQueryResult != detailedQueryResults.end() ; ++detailedQueryResult ) {
			log(
				boost::format(
					"%i:"
					"\tnumMatches %f\n"
					"\tdbMatchPercentage %f\n"
					"\tqueryMatchPercentage %f\n"
					"\tscore %f\n"
				)
				% detailedQueryResult->sceneModelIndex
				% detailedQueryResult->numMatches
				% detailedQueryResult->probeMatchPercentage
				% detailedQueryResult->queryMatchPercentage
				% detailedQueryResult->score
			);
		}

		return query.getQueryResults();
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
		editor.application = this;
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
		ProbeGenerator::initOrientations();

		settings.load();

		initMainWindow();
		initCamera();
		initEventHandling();

		initSGSInterface();

		probeDatabase.registerSceneModels( world->scene.modelNames );

		namedVolumesEditorView.reset( new NamedVolumesEditorView( sceneSettings.volumes ) );

		initEditor();

		eventDispatcher.addEventHandler( make_nonallocated_shared( widgetRoot ) );

		// register anttweakbar first because it actually manages its own focus
		antTweakBarEventHandler.init( mainWindow );
		eventDispatcher.addEventHandler( make_nonallocated_shared( antTweakBarEventHandler ) );

		// load sceneSettings
		sceneSettings.load( settings.sceneSettingsPath.c_str() );

		{
			// TODO: hack - fix this [10/20/2012 Andreas]
			const sf::Vector2i windowSize( mainWindow.getSize() );
			ViewportContext viewportContext( windowSize.x, windowSize.y );
			initUI();
		}

		ModelDatabase_init();
		neighborDatabaseV2.modelDatabase = &modelDatabase;

		if( !sceneSettings.views.empty() ) {
			sceneSettings.views.front().pushTo( mainCamera );
		}

		if( !sceneSettings.volumes.empty() ) {
			// TODO: add select wrappers to editorWrapper or editor [10/3/2012 kirschan2]
			editor.selectObb( 0 );
		}

		modelTypesUI->loadFromSceneSettings();
	}

	void Application::updateUI() {
		targetVolumesUI->update();
		cameraViewsUI->update();
		modelTypesUI->update();
		mainUI->update();

		modelSelectionBarUI->refresh();

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

				widgetRoot.transformChain.localTransform = Eigen::Scaling<float>( (float) windowSize.x, (float) windowSize.y, 1.0f );

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
		const float minDuration = 1.0f/25.0f; // target fps
		const float currentTime = clock.getElapsedTime().asSeconds();
		if( currentTime - lastUpdateTime < minDuration ) {
			return;
		}
		lastUpdateTime = currentTime;

		const sf::Vector2i windowSize( mainWindow.getSize() );

		mainWindow.pushGLStates();
		mainWindow.resetGLStates();
		glClearColor( 0.2f, 0.2f, 0.2f, 1.0f );
		mainWindow.clear();
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

		timedLog->updateTime( clock.getElapsedTime().asSeconds() );
		timedLog->updateText();
		timedLog->renderAsLog();

		sf::RectangleShape progressBar;
		progressBar.setPosition( 0.0f, windowSize.y * 0.95f );

		const float progressPercentage = ProgressTracker::getProgress();
		progressBar.setSize( sf::Vector2f( windowSize.x * progressPercentage, windowSize.y * 0.05f ) );
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

	void Application::NeighborhoodValidation_queryAllInstances( const std::string &filename ) {
		const int numModels = world->scene.models.size();
		const float maxDistance = sceneSettings.neighborhoodDatabase_maxDistance;
		const float positionVariance = settings.validation_neighborhood_positionVariance;
		const int numSamples = settings.validation_neighborhood_numSamples;

		boost::random::uniform_on_sphere<float, std::vector<float>> sphere(3);
		boost::random::uniform_real_distribution<float> squaredRadius( 0.0, positionVariance * positionVariance );

		Validation::NeighborhoodData data( numModels, Validation::NeighborhoodSettings( numSamples, maxDistance, positionVariance ) );

		const std::vector< int > markedModels = modelTypesUI->markedModels;
		for( auto markedModelIndex = markedModels.begin() ; markedModelIndex != markedModels.end() ; ++markedModelIndex ) {
			const auto instanceIndices = world->sceneRenderer.getModelInstances( *markedModelIndex );

			const int instanceIndexsCount = (int) instanceIndices.size();
			for( int instanceIndexIndex = 0 ; instanceIndexIndex < instanceIndexsCount ; instanceIndexIndex++ ) {
				const int instanceIndex = instanceIndices[ instanceIndexIndex ];

				const int modelIndex = world->sceneRenderer.getModelIndex( instanceIndex );
				data.instanceCounts.count( modelIndex );

				const auto position = world->sceneRenderer.getInstanceTransformation( instanceIndex ).translation();
				for( int sampleIndex = 0 ; sampleIndex < numSamples ; ++sampleIndex ) {
					const Vector3f shift =
							positionVariance > 0.0f
						?
							Vector3f( Vector3f::Map( &sphere( randomNumberGenerator ).front() ) * sqrtf( squaredRadius( randomNumberGenerator ) ) )
						:
							Eigen::Vector3f::Zero()
					;
					auto neighborContext = world->sceneGrid.query(
						-1,
						instanceIndex,
						position + shift,
						maxDistance
					);

					data.queryDatasets.push_back( neighborContext );
					data.queryInfos.push_back( modelIndex );
				}
			}
		}

		Validation::NeighborhoodData::store( filename, data );
	}

	void Application::ProbesValidation_determineQueryVolumeSizeForMarkedModels() const {
		const auto &modelIndices = modelTypesUI->markedModels;

		AlignedBox3f queryBoundingBox;
		AlignedBox3f offsetBoundingBox;

		const int numModels = modelIndices.size();

		ProgressTracker::Context modelProgressTracker( modelIndices.size() + 1 );
		for( int localModelIndex = 0 ; localModelIndex < numModels ; localModelIndex++ ) {
			const int sceneModelIndex = modelIndices[ localModelIndex ];

			auto instanceIndices = world->sceneRenderer.getModelInstances( sceneModelIndex );

			// loop through all instances of sceneModelIndex and create numSamples many query volumes
			for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
				const auto instanceRotation = Affine3f( world->sceneRenderer.getInstanceTransformation( *instanceIndex ).linear() );
				auto modelBoundingBox = world->sceneRenderer.getModelBoundingBox( sceneModelIndex );
				modelBoundingBox.extend( Vector3f::Zero() );
				offsetBoundingBox.extend( modelBoundingBox.center() );
				const auto instanceBoundingBox = Eigen_getTransformedAlignedBox( instanceRotation, modelBoundingBox );

				queryBoundingBox.extend( instanceBoundingBox );
			}
		}

		log( boost::format( "fit size: %f %f %f" )
			% queryBoundingBox.sizes()[0]
			% queryBoundingBox.sizes()[1]
			% queryBoundingBox.sizes()[2]
		);
		log( boost::format( "offset: %f %f %f" )
			% offsetBoundingBox.sizes()[0]
			% offsetBoundingBox.sizes()[1]
			% offsetBoundingBox.sizes()[2]
		);
	}

	void Application::ProbesValidation_queryAllInstances( const std::string &filename ) {
		AUTO_TIMER_FOR_FUNCTION();

		// we use the marked models as basis
		// its the only model group I support atm

		Validation::ProbeSettings probeSettings;

		probeSettings.colorTolerance = sceneSettings.probeQuery_colorTolerance;
		probeSettings.distanceTolerance = sceneSettings.probeQuery_distanceTolerance;
		probeSettings.occlusionTolerance = sceneSettings.probeQuery_occlusionTolerance;

		const float maxDistance = probeSettings.maxDistance = sceneSettings.probeGenerator_maxDistance;
		const float resolution = probeSettings.resolution = sceneSettings.probeGenerator_resolution;

		const float queryVolumeSize = probeSettings.queryVolumeSize = settings.validation_probes_queryVolumeSize;
		const int numSamples = probeSettings.numSamples = settings.validation_probes_numSamples;
		const float positionVariance = probeSettings.positionVariance = settings.validation_probes_positionVariance;

		boost::random::uniform_on_sphere<float, std::vector<float>> sphere(3);
		boost::random::uniform_real_distribution<float> squaredRadius( 0.0, positionVariance * positionVariance );

		RenderContext renderContext;
		renderContext.setDefault();

		// this will take some time
		startLongOperation();

		const auto &modelIndices = modelTypesUI->markedModels;

		const int numModels = modelIndices.size();
		Validation::ProbeData data( numModels, probeSettings );

		data.localModelNames = world->scene.modelNames;

		ProgressTracker::Context modelProgressTracker( modelIndices.size() + 1 );
		for( int localModelIndex = 0 ; localModelIndex < numModels ; localModelIndex++ ) {
			const int sceneModelIndex = modelIndices[ localModelIndex ];

			log( boost::format( "sampling model %i" ) % sceneModelIndex );

			auto modelBoundingBox = world->sceneRenderer.getModelBoundingBox( sceneModelIndex );
			modelBoundingBox.extend( Vector3f::Zero() );

			auto instanceIndices = world->sceneRenderer.getModelInstances( sceneModelIndex );
			data.instanceCounts.count( localModelIndex, instanceIndices.size() );

			// loop through all instances of sceneModelIndex and create numSamples many query volumes
			ProgressTracker::Context instanceProgressTracker( instanceIndices.size() );
			for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
				const auto position = world->sceneRenderer.getInstanceTransformation( *instanceIndex ).translation();

				for( int sampleIndex = 0 ; sampleIndex < numSamples ; ++sampleIndex ) {
					const Vector3f shift =
							positionVariance > 0
						?
							Vector3f( Vector3f::Map( &sphere( randomNumberGenerator ).front() ) * sqrtf( squaredRadius( randomNumberGenerator ) ) )
						:
							Eigen::Vector3f::Zero()
					;

					Validation::ProbeData::QueryData queryData;

					queryData.expectedSceneModelIndex = sceneModelIndex;
					//queryData.queryVolume = Obb( Obb::Transformation( Eigen::Translation3f( position + shift ) ), Eigen::Vector3f::Constant( queryVolumeSize ) );
					
					auto instanceBoundingBox = makeOBB( world->sceneRenderer.getInstanceTransformation( *instanceIndex ), modelBoundingBox );
					instanceBoundingBox.size += Vector3f::Constant( 0.25 );
					queryData.queryVolume = instanceBoundingBox;

					ProbeGenerator::generateQueryProbes( queryData.queryVolume.size, resolution, queryData.queryProbes );

					AUTO_TIMER_BLOCK( "sampling scene") {
						OptixRenderer::TransformedProbes transformedQueryProbes;
						ProbeGenerator::transformProbes( queryData.queryProbes, queryData.queryVolume.transformation, resolution, transformedQueryProbes );

						renderContext.disabledInstanceIndex = *instanceIndex;
						world->optixRenderer.sampleProbes( transformedQueryProbes, queryData.querySamples, renderContext, maxDistance );
					}
					instanceProgressTracker.markFinished();

					data.queries.push_back( std::move( queryData ) );
				}
			}

			modelProgressTracker.markFinished();
		}

		Validation::ProbeData::store( filename, data );
		modelProgressTracker.markFinished();

		endLongOperation();
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