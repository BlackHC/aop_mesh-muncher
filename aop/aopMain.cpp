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

const float neighborhoodMaxDistance = 100.0;
ModelDatabase modelDatabase;
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

struct EigenVector3fUIFactory : AntTWBarUI::SimpleStructureFactory< Eigen::Vector3f, EigenVector3fUIFactory > {
	template< typename Accessor >
	void setup( AntTWBarUI::Container *container, Accessor &accessor ) const {
		container->add(
			AntTWBarUI::makeSharedVariable(
				"x",
				AntTWBarUI::makeLinkedExpressionAccessor<float>( 
					[&] () -> float & {
						return accessor.pull().x();
					},
					accessor
				)
			)
		);
		container->add(
			AntTWBarUI::makeSharedVariable(
				"y",
				AntTWBarUI::makeLinkedExpressionAccessor<float>(
					[&] () -> float & {
						return accessor.pull().y();
					},
					accessor
				)
			)
		);
		container->add(
			AntTWBarUI::makeSharedVariable(
				"z",
				AntTWBarUI::makeLinkedExpressionAccessor<float>(
					[&] () -> float & {
						return accessor.pull().z();
					},
					accessor
				)
			)
		);
	}
};

struct EigenRotationMatrix : AntTWBarUI::SimpleStructureFactory< Eigen::Matrix3f, EigenRotationMatrix > {
	template< typename Accessor >
	void setup( AntTWBarUI::Container *container, Accessor &accessor ) const {
		container->add(
			AntTWBarUI::makeSharedVariable(
				"rotation",
				AntTWBarUI::CallbackAccessor< AntTWBarUI::Types::Quat4f >(
					[&] ( AntTWBarUI::Types::Quat4f &shadow ) {
						const Eigen::Quaternionf quat( accessor.pull() );
						for( int i = 0 ; i < 4 ; i++ ) {
							shadow.coeffs[i] = quat.coeffs()[i];
						}
					},
					[&] ( const AntTWBarUI::Types::Quat4f &shadow ) {
						Eigen::Quaternionf quat;
						for( int i = 0 ; i < 4 ; i++ ) {
							quat.coeffs()[i] = shadow.coeffs[i];
						}
						accessor.pull() = quat.toRotationMatrix();
						accessor.push();
					}
				)
			)
		);
	}
};

aop::Application::TimedLog::TimedLog( Application *application ) : application( application ) {
	init();
}

void aop::Application::TimedLog::init() {
	notifyApplicationOnMessage = false;
	rebuiltNeeded = false;
	totalHeight = 0.0;

	entries.resize( MAX_NUM_ENTRIES );

	Log::addSink(
		[this] ( int scope, const std::string &message, Log::Type type ) -> bool {
			if( this->size() >= MAX_NUM_ENTRIES - 1 ) {
				++beginEntry;
			}
			Entry &entry = entries[endEntry];
			entry.timeStamp = application->clock.getElapsedTime().asSeconds();

			entry.renderText.setCharacterSize( 15 );
			entry.renderText.setString( Log::Utility::indentString( scope, message, boost::str( boost::format( "[%s] " ) % entry.timeStamp ) ) );
			
			++endEntry;

			rebuiltNeeded = true;

			if( notifyApplicationOnMessage ) {
				application->updateProgress();
			}

			return true;
		}
	);

	
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
	log( boost::format( "sampling model %i" ) % modelIndex );

	RenderContext renderContext;
	renderContext.setDefault();
	//renderContext.disabledModelIndex = modelIndex;

	auto instanceIndices = world->sceneRenderer.getModelInstances( modelIndex );

	ProgressTracker::Context progressTracker( instanceIndices.size() * 3 );

	int totalCount = 0;

	for( int i = 0 ; i < instanceIndices.size() ; i++ ) {
		const int instanceIndex = instanceIndices[ i ];

		renderContext.disabledInstanceIndex = instanceIndex;

		RawProbeDataset rawDataset;
		std::vector<SGSInterface::Probe> transformedProbes;

		world->generateProbes( instanceIndex, probeResolution, rawDataset.probes, transformedProbes );

		progressTracker.markFinished();

		AUTO_TIMER_DEFAULT( boost::str( boost::format( "batch with %i probes for instance %i" ) % transformedProbes.size() % instanceIndex ) );

		world->optixRenderer.sampleProbes( transformedProbes, rawDataset.probeContexts, renderContext );

		progressTracker.markFinished();

		candidateFinder.addDataset(modelIndex, std::move( rawDataset ) );

		progressTracker.markFinished();

		totalCount += (int) transformedProbes.size();
	}

	log( boost::format( "total sampled probes: %i" ) % totalCount );
}

ProbeDatabase::Query::MatchInfos queryVolume( SGSInterface::World *world, ProbeDatabase &candidateFinder, const Obb &queryVolume ) {
	ProgressTracker::Context progressTracker(4);

	AUTO_TIMER_FOR_FUNCTION();

	RenderContext renderContext;
	renderContext.setDefault();

	RawProbeDataset rawDataset;

	ProbeGenerator::generateQueryProbes( queryVolume, probeResolution, rawDataset.probes );

	progressTracker.markFinished();

	{
		AUTO_TIMER_FOR_FUNCTION( "sampling scene");
		world->optixRenderer.sampleProbes( rawDataset.probes, rawDataset.probeContexts, renderContext );
	}
	progressTracker.markFinished();

	auto query = candidateFinder.createQuery();
	{
		query->setQueryDataset( std::move( rawDataset ) );

		ProbeContextTolerance pct;
		pct.setDefault();
		query->setProbeContextTolerance( pct );

		query->execute();
	}
	progressTracker.markFinished();

	const auto &matchInfos = query->getCandidates();
	for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
		log( boost::format( "%i: %f" ) % matchInfo->id % matchInfo->numMatches );
	}
	progressTracker.markFinished();

	return matchInfos;
}

// TODO: this should get its own file [9/30/2012 kirschan2]
EventSystem *EventHandler::eventSystem;

namespace aop {
	struct Application::CandidateSidebar {
		Application *application;

		struct CandidateContainer : WidgetContainer {
			float minY, maxY;
			float scrollStep;

			CandidateContainer() : minY(), maxY(), scrollStep() {}

			void onMouse( EventState &eventState ) {
				if( eventState.event.type == sf::Event::MouseWheelMoved ) {
					//log( boost::format( "mouse wheel moved %i" ) % eventState.event.mouseWheel.delta );
					
					auto offset = transformChain.getOffset();
					offset.y() = std::min( -minY, std::max( -maxY, offset.y() + eventState.event.mouseWheel.delta * scrollStep ) );
					transformChain.setOffset( offset );

					eventState.accept();
				}
				else {
					WidgetContainer::onMouse( eventState );
				}
			}
		};
		
		CandidateContainer sidebar;

		struct CandidateModelButton : ModelButtonWidget {
			CandidateModelButton(
				const Eigen::Vector2f &offset,
				const Eigen::Vector2f &size,
				int modelIndex,
				SGSSceneRenderer &renderer,
				const std::function<void()> &action = nullptr
			) :
				ModelButtonWidget( offset, size, modelIndex, renderer ),
				action( action )
			{}

			std::function<void()> action;

		private:
			void onAction() {
				if( action ) {
					action();
				}
			}
		};

		CandidateSidebar( Application *application ) : application( application ) {
			init();
		}

		void init() {				
			application->widgetRoot.addEventHandler( make_nonallocated_shared( sidebar ) );
		}

		void clear() {
			for( auto element = sidebar.eventHandlers.begin() ; element != sidebar.eventHandlers.end() ; ++element ) {
				application->eventSystem.onEventHandlerRemove( element->get() );
			}
			sidebar.eventHandlers.clear();
		}

		void addModels( std::vector<int> modelIndices, const Eigen::Vector3f &position ) {
			const float buttonWidth = 0.1;
			const float buttonAbsPadding = 32;
			const float buttonVerticalPadding = buttonAbsPadding / ViewportContext::context->framebufferHeight;
			const float buttonHorizontalPadding = buttonAbsPadding / ViewportContext::context->framebufferWidth;

			// TODO: this was in init but meh
			sidebar.transformChain.setOffset( Eigen::Vector2f( 1 - buttonHorizontalPadding - buttonWidth, 0 ) );

			const float buttonHeight = buttonWidth * ViewportContext::context->getAspectRatio();
			const float buttonHeightWithPadding = buttonHeight + buttonVerticalPadding;

			const int maxNumModels = 30;

			// add 5 at most
			const int numModels = std::min<int>( maxNumModels, modelIndices.size() );

			sidebar.minY = 0;
			sidebar.maxY = (numModels - 1 ) * buttonHeightWithPadding;
			sidebar.scrollStep = buttonHeightWithPadding;
			
			for( int i = 0 ; i < numModels ; i++ ) {
				const int modelIndex = modelIndices[ i ];

				sidebar.addEventHandler(
					std::make_shared< CandidateModelButton >(
						Eigen::Vector2f( 0.0, buttonVerticalPadding + i * buttonHeightWithPadding ),
						Eigen::Vector2f( buttonWidth, buttonHeight ),
						modelIndex,
						application->world->sceneRenderer,
						[this, modelIndex, position] () {
							if( sf::Keyboard::isKeyPressed( sf::Keyboard::LAlt ) ) {
								log( application->world->scene.modelNames[ modelIndex ] );
								application->editor.selectModel( modelIndex );
							}
							else {
								application->world->addInstance( modelIndex, position );
							}
						}
					)
				);
			}
		}
	};

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

	struct Application::ModelTypesUI {
		Application *application;

		std::shared_ptr< AntTWBarUI::SimpleContainer > modelsUi;
		AntTWBarUI::SimpleContainer markedModelsUi;

		std::vector< std::string > beautifiedModelNames;
		std::vector< int > markedModels;

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
			modelsUi = std::make_shared< AntTWBarUI::Vector< ModelNameView, MyConfig > >( "All models", beautifiedModelNames, ModelNameView( this ), AntTWBarUI::CT_EMBEDDED );
			modelsUi->link();

			markedModelsUi.setName( "Marked models");
			auto markedModelsVector = AntTWBarUI::makeSharedVector( "Models", markedModels, MarkedModelNameView( this ), AntTWBarUI::CT_EMBEDDED );
			markedModelsUi.add( markedModelsVector );
			markedModelsUi.add( AntTWBarUI::makeSharedSeparator() );
			markedModelsUi.add( AntTWBarUI::makeSharedButton( "= {}", [this] () {
				markedModels.clear();
			} ) );
			markedModelsUi.add( AntTWBarUI::makeSharedButton( "= selection", [this] () {
				ReplaceWithSelectionVisitor( this ).dispatch( application->editor.selection );
			} ) );
			markedModelsUi.add( AntTWBarUI::makeSharedButton( "+= selection", [this] () {
				AppendSelectionVisitor( this ).dispatch( application->editor.selection );
			} ) );
			markedModelsUi.add( AntTWBarUI::makeSharedSeparator() );
			markedModelsUi.add( AntTWBarUI::makeSharedButton( "selection =", [this] () {
				application->editor.selectModels( markedModels );
			} ) );
			markedModelsUi.link();
		}

		void update() {
			modelsUi->refresh();
			markedModelsUi.refresh();
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
					sampleInstances( application->world.get(), application->candidateFinder, *modelIndex );
					progressTracker.markFinished();
				}
				
				application->candidateFinder.integrateDatasets();
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
						auto matchInfos = queryVolume( application->world.get(), application->candidateFinder, selection->getObb() );
						application->endLongOperation();

						typedef ProbeDatabase::Query::MatchInfo MatchInfo;
						boost::sort( 
							matchInfos,
							[] (const MatchInfo &a, MatchInfo &b ) {
								return a.numMatches > b.numMatches;
							}
						);

						std::vector<int> modelIndices;
						for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
							modelIndices.push_back( matchInfo->id );	
						}

						application->candidateSidebar->clear();
						application->candidateSidebar->addModels( modelIndices, selection->getObb().transformation.translation() );
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

						application->candidateSidebar->clear();
						application->candidateSidebar->addModels( modelIndices, selection->getObb().transformation.translation() );
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

						application->candidateSidebar->clear();
						application->candidateSidebar->addModels( modelIndices, selection->getObb().transformation.translation() );
					}
				};
				QueryNeighborsVisitor( application ).dispatch( application->editor.selection );
			} ) );
			ui.add( AntTWBarUI::makeSharedSeparator() );
			ui.add( AntTWBarUI::makeSharedButton( "Load database", [this] { application->candidateFinder.loadCache( "database"); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Reset database", [this] { application->candidateFinder.reset(); } ) );
			ui.add( AntTWBarUI::makeSharedButton( "Store database", [this] { application->candidateFinder.storeCache( "database"); } ) );
			ui.link();
		}

		void update() {
			ui.refresh();
		}
	};

	struct Application::TargetVolumesUI {
		Application *application;

		AntTWBarUI::SimpleContainer ui;

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
					EigenRotationMatrix().makeShared( 
						AntTWBarUI::CallbackAccessor<Eigen::Matrix3f>(
							[&] ( Eigen::Matrix3f &shadow ) {
								shadow = accessor.pull().volume.transformation.linear();
							},
							[&] ( const Eigen::Matrix3f &shadow ) {
								accessor.pull().volume.transformation.linear() = shadow;
							}
						),
						AntTWBarUI::CT_EMBEDDED
					)
				);
				container->add(
					EigenVector3fUIFactory().makeShared( 
						AntTWBarUI::CallbackAccessor<Eigen::Vector3f>(
							[&] ( Eigen::Vector3f &shadow ) {
								shadow = accessor.pull().volume.transformation.translation();
							},
							[&] ( const Eigen::Vector3f &shadow ) {
								accessor.pull().volume.transformation.translation() = shadow;
							}
						),
						AntTWBarUI::CT_GROUP
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Select",
						[&] () {
							targetVolumesUI->application->editor.selectObb( accessor.elementIndex );
						}
					)
				);
			}
		};

		TargetVolumesUI( Application *application ) : application( application ) {
			init();
		}

		void init() {
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
					}
				)
			);
			ui.link();
		}

		void update() {
			ui.refresh();
		}

	};

	struct Application::CameraViewsUI {
		Application *application;

		AntTWBarUI::SimpleContainer ui;

		struct NamedCameraStateView : AntTWBarUI::SimpleStructureFactory< aop::Settings::NamedCameraState, NamedCameraStateView >{
			Application *application;

			NamedCameraStateView( Application *application ) : application( application ) {}

			template< typename ElementAccessor >
			void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
				container->add(
					AntTWBarUI::makeSharedVariable(
						"Name",
						AntTWBarUI::makeMemberAccessor( accessor, &aop::Settings::NamedCameraState::name )
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Set default",
						[&] () {
							auto &views = application->settings.views;
							std::swap( views.begin(), views.begin() + accessor.elementIndex );
						}
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Use",
						[&] () {
							accessor.pull().pushTo( this->application->mainCamera );
						}
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Replace",
						[&] () {
							accessor.pull().pullFrom( this->application->mainCamera );
							accessor.push();
						}
					)
				);
			}
		};

		CameraViewsUI( Application *application ) : application( application ) {
			init();
		}

		void init() {
			ui.setName( "Camera views" );

			ui.add( AntTWBarUI::makeSharedButton(
					"Add current view",
					[this] () {
						application->settings.views.push_back( aop::Settings::NamedCameraState() );
						application->settings.views.back().pullFrom( application->mainCamera );
					}
				)
			);
			ui.add( AntTWBarUI::makeSharedButton(
					"Clear all",
					[this] () {
						application->settings.views.clear();
					}
				)
			);

			auto cameraStatesView = AntTWBarUI::makeSharedVector( application->settings.views, NamedCameraStateView( application ) );
			ui.add( cameraStatesView );

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
		candidateSidebar.reset( new CandidateSidebar( this ) );
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

		{
			const auto &models = world->scene.models;
			const int numModels = models.size();

			for( int modelId = 0 ; modelId < numModels ; modelId++ ) {
				const auto bbox = world->sceneRenderer.getModelBoundingBox( modelId );

				ModelDatabase::IdInformation idInformation;
				const Vector3f sizes = bbox.sizes();
				idInformation.diagonalLength = sizes.norm();
				// sucks for IND## idInformation.area = sizes.prod() * sizes.cwiseInverse().sum() * 2;
				idInformation.area =
					2 * sizes[0] * sizes[1] +
					2 * sizes[1] * sizes[2] +
					2 * sizes[0] * sizes[2]
				;
				idInformation.volume = sizes.prod();

				modelDatabase.informationById.push_back( idInformation );
			}
		}
		// TODO XXX
		neighborDatabaseV2.modelDatabase = &modelDatabase;
	}

	void Application::initEventHandling() {
		eventDispatcher.name = "Input help:";
		eventSystem.rootHandler = make_nonallocated_shared( eventDispatcher );
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
		sampleAllNeighbors( neighborhoodMaxDistance, neighborDatabase, *world );
		sampleAllNeighborsV2( neighborhoodMaxDistance, neighborDatabaseV2, *world );

		candidateFinder.reserveIds( world->scene.modelNames.size() );

		namedVolumesEditorView.reset( new NamedVolumesEditorView( settings.volumes ) );

		initEditor();

		eventDispatcher.addEventHandler( make_nonallocated_shared( widgetRoot ) );

		// register anttweakbar first because it actually manages its own focus
		antTweakBarEventHandler.init( mainWindow );
		eventDispatcher.addEventHandler( make_nonallocated_shared( antTweakBarEventHandler ) );

		initUI();

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
	}

	void Application::eventLoop() {
		sf::Text renderDuration;
		renderDuration.setCharacterSize( 12 );

		KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { world->sceneRenderer.reloadShaders(); } );
		eventDispatcher.addEventHandler( make_nonallocated_shared( reloadShadersAction ) );

		while (true)
		{
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

				//world.renderOptixViewFrame( view );

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

	void Application::updateProgress() {
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