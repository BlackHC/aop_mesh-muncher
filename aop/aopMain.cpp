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

#include "contextHelper.h"
#include "boost/range/algorithm/copy.hpp"
#include "boost/range/adaptor/transformed.hpp"

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

struct TransformChain {
	Eigen::Affine3f localTransform, globalTransform;

	TransformChain() : localTransform( Eigen::Affine3f::Identity() ), globalTransform( Eigen::Affine3f::Identity() ) {}

	void update( TransformChain *parent = nullptr ) {
		if( parent ) {
			globalTransform = parent->globalTransform * localTransform;
		}
		else {
			globalTransform = localTransform;
		}
	}

	void setOffset( const Eigen::Vector2f &offset ) {
		localTransform = Eigen::Translation3f( Eigen::Vector3f( offset[0], offset[1], 0.0f ) );
	}

	Eigen::Vector2f pointToScreen( const Eigen::Vector2f &point ) const {
		return ( globalTransform * Eigen::Vector3f( point[0], point[1], 0.0f ) ).head<2>();
	}

	Eigen::Vector2f vectorToScreen( const Eigen::Vector2f &point ) const {
		return ( globalTransform.linear() * Eigen::Vector3f( point[0], point[1], 0.0f ) ).head<2>();
	}

	Eigen::Vector2f screenToPoint( const Eigen::Vector2f &point ) const {
		return ( globalTransform.inverse() * Eigen::Vector3f( point[0], point[1], 0.0f ) ).head<2>();
	}

	Eigen::Vector2f screenToVector( const Eigen::Vector2f &point ) const {
		return ( globalTransform.inverse().linear() * Eigen::Vector3f( point[0], point[1], 0.0f ) ).head<2>();
	}
};

// supports y down coordinates and sets the viewport automatically
struct ViewportContext : AsExecutionContext< ViewportContext > {
	// total size
	int framebufferWidth;
	int framebufferHeight;

	// viewport
	int left, top;
	int width, height;

	ViewportContext( int framebufferWidth, int framebufferHeight ) :
		framebufferWidth( framebufferWidth ), framebufferHeight( framebufferHeight ),
		left( 0 ), top( 0 ), width( framebufferWidth ), height( framebufferHeight )
	{
		updateGLViewport();
	}

	ViewportContext( int left, int top, int width, int height ) :
		left( left ), top( top ), width( width ), height( height )
	{
		updateGLViewport();
	}

	ViewportContext() : AsExecutionContext( ExpectNonEmpty() ) {}

	void onPop() {
		updateGLViewport();
	}

	void updateGLViewport() {
		glViewport( left, framebufferHeight - (top + height), width, height );
	}

	float getAspectRatio() const {
		return float( framebufferWidth ) / framebufferHeight;
	}

	// relative coords are 0..1
	Eigen::Vector2f screenToViewport( const Eigen::Vector2i &screen ) const {
		return Eigen::Vector2f( (screen[0] - left) / float( width ), (screen[1] - top) / float( height ) );
	}
};

struct ITransformChain : virtual EventHandler {
	TransformChain transformChain;
};

struct IWidget : virtual ITransformChain, virtual EventHandler::WithParentDecl< ITransformChain > {
	virtual void onRender() = 0;
};

// TODO: this is a huge cluster fuck (together with WidgetRoot) and way too dependent on implementation details.. [10/4/2012 kirschan2]
struct WidgetBase : TemplateNullEventHandler< ITransformChain, IWidget > {
	void onRender() {
		glPushMatrix();
		Eigen::glLoadMatrix( transformChain.globalTransform );
		doRender();
		glPopMatrix();
	}

	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
		transformChain.update( &parent->transformChain );

		doUpdate( eventSystem, frameDuration, elapsedTime );
	}

private:
	virtual void doRender() = 0;
	virtual void doUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) = 0;
};

struct WidgetContainer : TemplateEventDispatcher< IWidget, EventHandler::WithSimpleParentImpl< ITransformChain, IWidget > > {
	void onRender() {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			eventHandler->get()->onRender();
		}
	}

	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
		transformChain.update( &parent->transformChain );

		Base::onUpdate( eventSystem, frameDuration, elapsedTime );
	}
};

struct WidgetRoot : TemplateEventDispatcher< IWidget, EventHandler::WithSimpleParentImpl<EventHandler, ITransformChain> > {
	void onRender() {
		for( auto eventHandler = eventHandlers.rbegin() ; eventHandler != eventHandlers.rend() ; ++eventHandler ) {
			eventHandler->get()->onRender();
		}
	}

	void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
		transformChain.update();

		Base::onUpdate( eventSystem, frameDuration, elapsedTime );
	}
};

struct ButtonWidget : WidgetBase {
	enum State {
		STATE_INACTIVE,
		STATE_HOVER,
		STATE_CLICKED
	} state;

	Eigen::Vector2f size;

	ButtonWidget( const Eigen::Vector2f &offset, const Eigen::Vector2f &size ) : state(), size( size ) {
		transformChain.setOffset( offset );
	}

	bool isInArea( const Eigen::Vector2f &globalPosition ) {
		const Eigen::Vector2f localPosition = transformChain.screenToPoint( globalPosition );
		if( localPosition[0] >= 0 && localPosition[1] >= 0 &&
			localPosition[0] <= size[0] && localPosition[1] <= size[1]
		) {
			return true;
		}
		return false;
	}

	void setState( State newState ) {
		if( state == newState ) {
			return;
		}

		if( newState == STATE_CLICKED ) {
			onAction();
		}

		if( newState == STATE_INACTIVE ) {
			onMouseLeave();
		}
		else if( state == STATE_INACTIVE ) {
			onMouseEnter();
		}

		state = newState;
	}

	void onMouse( EventState &eventState ) {
		bool hasMouse = eventState.getCapture( this ) == FT_MOUSE;
		switch( eventState.event.type ) {
		case sf::Event::MouseMoved:
			if( isInArea( Eigen::Vector2f( eventState.event.mouseMove.x, eventState.event.mouseMove.y ) ) ) {
				eventState.setCapture( this, FT_MOUSE );

				if( !hasMouse && state != STATE_CLICKED ) {
					setState( STATE_HOVER );
				}

				eventState.accept();
			}
			else if( !sf::Mouse::isButtonPressed( sf::Mouse::Left ) ){
				eventState.setCapture( this, FT_NONE );
				setState( STATE_INACTIVE );
			}

			break;
		case sf::Event::MouseButtonPressed:
			if( hasMouse ) {
				onAction();
				setState( STATE_CLICKED );
				eventState.accept();
			}
			break;
		case sf::Event::MouseButtonReleased:
			if( hasMouse ) {
				eventState.accept();
			}
			if( state == STATE_CLICKED ) {
				if( isInArea( Eigen::Vector2f( eventState.event.mouseButton.x, eventState.event.mouseButton.y ) ) ) {
					setState( STATE_HOVER );
				}
				else {
					setState( STATE_INACTIVE );
				}
			}
			break;
		}
	}

	void doUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
	}

	bool acceptFocus( FocusType focusType ) {
		if( focusType == FT_MOUSE ) {
			return true;
		}
		return false;
	}

private:
	void doRender() {
		switch( state ) {
		case STATE_HOVER:
			DebugRender::setColor( Vector3f( 1.0, 1.0, 1.0 ) );
			break;
		case STATE_CLICKED:
			DebugRender::setColor( Vector3f( 1.0, 0.0, 0.0 ) );
			break;
		default:
			DebugRender::setColor( Vector3f( 1.0, 0.0, 1.0 ) );
			break;
		}

		DebugRender::drawAABB( Vector3f::Zero(), Vector3f( size[0], size[1], 0.0f ) );

		doRenderInside();
	}

	virtual void onAction() = 0;
	virtual void doRenderInside() = 0;
	virtual void onMouseEnter() = 0;
	virtual void onMouseLeave() = 0;
};

struct DummyButtonWidget : ButtonWidget {
	void onAction() {}
	void onMouseEnter() {}
	void onMouseLeave() {}
	void doRenderInside() {}
};

struct ModelButtonWidget : ButtonWidget {
	SGSSceneRenderer &renderer;
	int modelIndex;

	// use to rotate the object continuously
	float time;

	ModelButtonWidget( const Eigen::Vector2f &offset, const Eigen::Vector2f &size, int modelIndex, SGSSceneRenderer &renderer ) :
		ButtonWidget( offset, size ),
		modelIndex( modelIndex ),
		renderer( renderer ),
		time( 0 )
	{}

	void doUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
		time = elapsedTime;
	}

	void doRenderInside() {
		const Eigen::Vector2f realMinCorner = transformChain.pointToScreen( Eigen::Vector2f::Zero() );
		const Eigen::Vector2f realSize = transformChain.vectorToScreen( size );

		// init the persective matrix
		glMatrixMode( GL_PROJECTION );
		glPushMatrix();
		glLoadMatrix( Eigen::createPerspectiveProjectionMatrix( 60.0, realSize[0] / realSize[1], 0.001, 100.0 ) );

		glMatrixMode( GL_MODELVIEW );
		glPushMatrix();
		const auto worldViewerPosition = Vector3f( 0.0, 1.0, 1.0 );
		glLoadMatrix( Eigen::createLookAtMatrix( worldViewerPosition, Vector3f::Zero(), Vector3f::UnitY() ) );

		// see note R3 for the computation
		const Eigen::Vector3f boundingBoxSize =  renderer.getModelBoundingBox(modelIndex).sizes();
		Eigen::glScale( Vector3f::Constant( sqrt( 2.0 / 3.0 ) / boundingBoxSize.norm() ) );

		float angle = time * 2 * Math::PI / 10.0;
		glMultMatrix( Affine3f(AngleAxisf( angle, Vector3f::UnitY() )).matrix() );

		// TODO: scissor test [10/4/2012 kirschan2]

		glClear( GL_DEPTH_BUFFER_BIT );
		glEnable( GL_DEPTH_TEST );

		{
			ViewportContext viewport( realMinCorner[0], realMinCorner[1], realSize[0], realSize[1] );

			renderer.renderModel( worldViewerPosition, modelIndex );
		}

		glMatrixMode( GL_PROJECTION );
		glPopMatrix();

		glMatrixMode( GL_MODELVIEW );
		glPopMatrix();
	}
	
	void onAction() {}
	void onMouseEnter() {}
	void onMouseLeave() {}
};

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

		RawProbeDataset rawDataset;
		std::vector<SGSInterface::Probe> transformedProbes;

		world->generateProbes( instanceIndex, probeResolution, rawDataset.probes, transformedProbes );

		AUTO_TIMER_DEFAULT( boost::str( boost::format( "batch with %i probes for instance %i" ) % transformedProbes.size() % instanceIndex ) );

		world->optixRenderer.sampleProbes( transformedProbes, rawDataset.probeContexts, renderContext );

		candidateFinder.addDataset(modelIndex, std::move( rawDataset ) );

		totalCount += (int) transformedProbes.size();
	}

	std::cerr << Indentation::get() << "total sampled probes: " << totalCount << "\n";
}

ProbeDatabase::Query::MatchInfos queryVolume( SGSInterface::World *world, ProbeDatabase &candidateFinder, const Obb &queryVolume ) {
	AUTO_TIMER_FOR_FUNCTION();

	RenderContext renderContext;
	renderContext.setDefault();

	RawProbeDataset rawDataset;

	ProbeGenerator::generateQueryProbes( queryVolume, probeResolution, rawDataset.probes );

	{
		AUTO_TIMER_FOR_FUNCTION( "sampling scene");
		world->optixRenderer.sampleProbes( rawDataset.probes, rawDataset.probeContexts, renderContext );
	}

	auto query = candidateFinder.createQuery();
	{
		query->setQueryDataset( std::move( rawDataset ) );

		ProbeContextTolerance pct;
		pct.setDefault();
		query->setProbeContextTolerance( pct );

		query->execute();
	}

	const auto &matchInfos = query->getCandidates();
	for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
		std::cout << Indentation::get() << matchInfo->id << ": " << matchInfo->numMatches << "\n";
	}
	return matchInfos;
}

// TODO: this should get its own file [9/30/2012 kirschan2]
EventSystem *EventHandler::eventSystem;

namespace aop {
	struct Application {
		sf::RenderWindow mainWindow;

		EventSystem eventSystem;
		EventDispatcher eventDispatcher;
		AntTweakBarEventHandler antTweakBarEventHandler;

		WidgetRoot widgetRoot;

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
							auto matchInfos = queryVolume( application->world.get(), application->candidateFinder, selection->getObb() );

							typedef ProbeDatabase::Query::MatchInfo MatchInfo;
							boost::sort( matchInfos, [] (const MatchInfo &a, MatchInfo &b ) {
									return a.numMatches > b.numMatches;
								}
							);

							std::vector<int> modelIndices;
							for( auto matchInfo = matchInfos.begin() ; matchInfo != matchInfos.end() ; ++matchInfo ) {
								modelIndices.push_back( matchInfo->id );	
							}

							application->candidateSidebar->clear();
							application->candidateSidebar->addModels( modelIndices );
						}
					};
					QueryVolumeVisitor( application ).dispatch( application->editorWrapper->editor.selection.get() );
				} ) );
				ui.add( AntTWBarUI::makeSharedSeparator() );
				ui.add( AntTWBarUI::makeSharedButton( "Load database", [this] { application->candidateFinder.loadCache( "database"); } ) );
				ui.add( AntTWBarUI::makeSharedButton( "Store database", [this] { application->candidateFinder.storeCache( "database"); } ) );
				ui.link();
			}

			void update() {
				ui.refresh();
			}
		};

		struct TargetVolumesUI {
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

		struct CameraViewsUI {
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

		struct ModelTypesUI {
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
			}

			void update() {
				modelsUi->refresh();
				markedModelsUi.refresh();
			}
		};

		struct CandidateSidebar {
			Application *application;
			
			WidgetContainer sidebar;

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

			void addModels( std::vector<int> modelIndices ) {
				const float buttonWidth = 0.1;
				const float buttonPadding = 0.05;

				// TODO: this was in init but meh
				sidebar.transformChain.setOffset( Eigen::Vector2f( 1 - buttonPadding - buttonWidth, buttonPadding ) );

				const float buttonHeight = buttonWidth * ViewportContext::context->getAspectRatio();
				const float buttonHeightWithPadding = buttonHeight + buttonPadding;

				const int maxNumModels = (1.0 - buttonPadding) / buttonHeightWithPadding;

				// add 5 at most
				const int numModels = std::min<int>( maxNumModels, modelIndices.size() );

				for( int i = 0 ; i < numModels ; i++ ) {
					const int modelIndex = modelIndices[ i ];

					sidebar.addEventHandler(
						std::make_shared< ModelButtonWidget >(
							Eigen::Vector2f( 0.0, i * buttonHeightWithPadding ),
							Eigen::Vector2f( buttonWidth, buttonHeight ),
							modelIndex,
							application->world->sceneRenderer
						)
					);
				}
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
		std::unique_ptr< CandidateSidebar > candidateSidebar;

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
			candidateSidebar.reset( new CandidateSidebar( this ) );
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

			eventDispatcher.addEventHandler( make_nonallocated_shared( widgetRoot ) );

			/*auto button = std::make_shared<ModelButtonWidget>( Vector2f::Constant( 0 ), Vector2f::Constant( 0.25 ), 0, world->sceneRenderer );

			widgetRoot.addEventHandler( button );*/

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
				editorWrapper->editor.selection = std::make_shared< Editor::ObbSelection >( &editorWrapper->editor, 0 );
			}
		}

		void updateUI() {
			targetVolumesUI->update();
			cameraViewsUI->update();
			modelTypesUI->update();
			mainUI->update();
		}

		void eventLoop() {
			sf::Text renderDuration;
			renderDuration.setPosition( 0, 0 );
			renderDuration.setCharacterSize( 10 );

			sf::Clock frameClock, clock;

			KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { world->sceneRenderer.reloadShaders(); } );
			eventDispatcher.addEventHandler( make_nonallocated_shared( reloadShadersAction ) );

			{
				const sf::Vector2i windowSize( mainWindow.getSize() );
				ViewportContext viewportContext( windowSize.x, windowSize.y );

				std::vector<int> modelIndices;
				modelIndices.push_back( 0 );
				modelIndices.push_back( 1 );
				modelIndices.push_back( 2 );

				candidateSidebar->addModels( modelIndices );
			}

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